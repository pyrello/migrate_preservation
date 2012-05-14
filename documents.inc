<?php

/**
 * @file
 * Advanced migration examples. These serve two purposes:
 *
 * 1. To demonstrate some of the more advanced usages of the Migrate module.
 *    Search for "TIP:" below for features not found in the basic example.
 * 2. To provide thorough test cases for the simpletest suite.
 *
 */
 
class DocumentMigration extends XMLMigration {
	//private $sourceid;
  public function __construct() {
    parent::__construct();
    $this->description = t('XML feed (multi items) of roles (positions)');
		
    // There isn't a consistent way to automatically identify appropriate "fields"
    // from an XML feed, so we pass an explicit list of source fields
    $fields = array(
      'filename' => t('File name'),
      'title' => t('Publication title'),
      'country' => t('Publication country'),
      'state' => t('Publication state'),
			'city' => t('Publication city'),
			'date' => t('Publication date'),
			'lang' => t('Language'),
			'pagenum' => t('Page number'),
			'exceptions' => t('Exceptions'),
			'id' => t('Image ID'),
    );

    // The source ID here is the one retrieved from the XML listing file, and
    // used to identify the specific item's file
    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'id' => array(
          'type' => 'varchar',
					'length' => 255,
          'not null' => TRUE,
					'description' => 'The Image ID',
        )
      ),
      MigrateDestinationNode::getKeySchema()
    );

    // This can also be an URL instead of a file path.
    $xml_folder = variable_get('migrate_preservation_document_path_absolute', DRUPAL_ROOT);
		
		$files = file_scan_directory($xml_folder, '/\.xml$/', array('recurse' => FALSE));
		$items_url = array_keys($files);
	
		$item_xpath = '/reel/image';  // relative to document
    $item_ID_xpath = 'id';          // relative to item_xpath
		
    $this->source = new MigrateSourceXML($items_url, $item_xpath, $item_ID_xpath, $fields);
    $this->destination = new MigrateDestinationNode('document');
		$this->addFieldMapping('id');
		$this->addFieldMapping('sourceid');
		$this->addFieldMapping('field_document_docid', 'docid')
				->description('Populated by prepareRow');		
    $this->addFieldMapping('title', 'full_title')
				->description('Populated by prepareRow');		
		$this->addFieldMapping('field_document_country', 'country')
         ->xpath('country');
		$this->addFieldMapping('field_document_state', 'state')
         ->xpath('state');
		$this->addFieldMapping('field_document_city', 'city')
         ->xpath('city');
		$this->addFieldMapping('field_document_pubtitle', 'title')
         ->xpath('title');
		$this->addFieldMapping('field_document_pagenum', 'pagenum')
         ->xpath('pagenum');
		$this->addFieldMapping('field_document_text_raw', 'raw_text') // works!
				->description('Populated by prepareRow from a text file');	
		$this->addFieldMapping('field_document_dirid', 'directory_id')
				->description('Populated by prepareRow');
		$this->addFieldMapping('field_document_collection', 'collection')
				->description('Populated by prepareRow');		
		$this->addFieldMapping('field_document_filename', 'filename')
         ->xpath('filename');
		// For simple date fields, we just need the xpath
    $this->addFieldMapping('date')
         ->xpath('date');
		$this->addFieldMapping('field_document_pdf', 'pdf');
		
		// Date handled as separate fields because of limitations on php dates.
		$this->addFieldMapping('field_document_year', 'year');
		$this->addFieldMapping('field_document_month', 'month');
		$this->addFieldMapping('field_document_day', 'day');
  }
	
	public function prepareRow($row) {
		//dpm($row);
		$filename = (string) $row->xml->filename;
		//dpm($filename, 'filename - prepareRow');
		$path = $this->source->activeUrl();
		//dpm($path, 'path - prepareRow');
		$row->docid = $this->getDocId($path, $filename);
		$row->raw_text = $this->getTextFile($path, $filename);
		$row->directory_id = $this->getDirectoryId($path);
		$row->collection = $this->getCollection($path);
		$row->full_title = $this->getFullTitle($row);
		$row->pdf = $this->getPDF($path, $filename);
		//dpm((string) $row->xml->pagenum, 'pagenum - prepareRow');
		if ((string) $row->xml->pagenum == '') {
			$row->pagenum = 0;
		}
		$dateArr = explode('-', (string) $row->xml->date);
		//dpm($dateArr);
		$row->year = $dateArr[0];
		$row->month = $dateArr[1];
		$row->day = $dateArr[2];
	}
	
	public function getFullTitle($row) {
		return (string) $row->xml->title . ' ' . (string) $row->xml->date . ' - Page ' . (string) $row->xml->pagenum;
	}
	
	public function getDocId($xmlpath, $scanfilename) {
		$file = basename($xmlpath);
		$filename = substr($file, 0, strrpos($file, '.'));
		$parts = explode('@#@', $filename);
		$docid = $parts[0] . '--' . $parts[1] . '--' . $scanfilename;
		return $docid;
	}
	
	public function getTextFile($xmlpath, $scanfilename) {
		$path = $this->getFilePath($xmlpath, $scanfilename, '.txt');
		$data = file_exists($path) ? utf8_encode(file_get_contents($path)) : '';
		return $data;
	}
	
	public function getPDF($xmlpath, $scanfilename) {
		$path = $this->getFilePath($xmlpath, $scanfilename, '.pdf');
		$pdf = array();
		$pdf['path'] = $path;
		$pdf = drupal_json_encode($pdf);
		return $pdf;
	}
	
	public function getFilePath($xmlpath, $filename, $ext) {
		$xmlfile = basename($xmlpath);
		$xmlname = substr($xmlfile, 0, strrpos($xmlfile, '.'));
		$parts = explode('@#@', $xmlname);
		$path = variable_get('migrate_preservation_document_path_absolute', DRUPAL_ROOT) . '/' . $parts[0] . '/';
		
		if (file_exists($path . $parts[1] . '/' . $parts[1]  . '/' . $filename . $ext)) { // Example: /Collection/Dates/Dates/File.ext
			$path = $path . $parts[1] . '/' . $parts[1]  . '/' . $filename . $ext;
		} else if (file_exists($path . $parts[1] . '/' . $filename . $ext)) { // Example: /Collection/Dates/File.ext
			$path = $path . $parts[1] . '/' . $filename . $ext;
		} else if (file_exists($path . $filename . $ext)) { // Example: /Collection/File.ext
			$path = $path . $filename . $ext;
		} else { // Can't find the file
			return FALSE;
		}
		return $path;
	}
	
	public function getDirectoryId($xmlpath) {
		$file = basename($xmlpath);
		$filename = substr($file, 0, strrpos($file, '.'));
		$parts = explode('@#@', $filename);
		return $parts[1];
	}
	
	public function getCollection($xmlpath) {
		$file = basename($xmlpath);
		$filename = substr($file, 0, strrpos($file, '.'));
		$parts = explode('@#@', $filename);
		return $parts[0];
	}
	
}