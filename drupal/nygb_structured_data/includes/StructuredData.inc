<?php

class StructuredData {
  /**
   * Prepare file to be stored.
   *
   * @param $headers
   *   Header array
   * @param $values
   *   Values array
   *
   * @return bool|false|string
   *
   * @throws \StructuredDataException
   */
  static public function prepareForStorage(&$row) {
    $keys = self::dataCleanup($row);
    $data_keys = self::extractKeyData($row, $keys);
    if (!$data_keys) {
      return FALSE;
    }
    self::generateJSON($row, $data_keys);
    return TRUE;
  }

  static protected function dataCleanup(&$row) {
    $data_keys = array_keys(get_object_vars($row));
    $variants = [
      'last_name' => ['lastname', 'surname', 'family_name'],
      'first_name' => ['firstname', 'given_name'],
      'image_number' => ['page', 'page_number'],
    ];
    foreach($variants as $source => $variant_set) {
      if (!isset($row->{$source})) {
        foreach($variant_set as $variant) {
          if (isset($row->{$variant})) {
            $row->{$source} = $row->{$variant};
            $position = array_search($variant, $data_keys);
            $data_keys[$position] = $source;
            unset($row->{$variant});
            break;
          }
        }
      }
    }
    return $data_keys;
  }

  /**
   * Generate JSON data for storage
   *
   * @param $headers
   * @param $values
   *
   * @return bool|false|string
   */
  static protected function generateJSON(&$row, $data_keys) {
    $data = [];
    foreach($data_keys as $key => $blank) {
      $data[$key] = $row->{$key};
    }
    $row->data = json_encode($data);
  }

  /**
   * Extract significant key data that isn't going to be just wrapped in JSON.
   *
   * @param $dataset
   *
   * @return mixed
   * @throws \StructuredDataException
   */
  static protected function extractKeyData(&$row, $keys) {
    $data_keys = array_flip($keys);
    unset($data_keys['csvrownum']);
    if (!isset($row->collection_id) || !is_numeric($row->collection_id)) {
      throw new StructuredDataException("Missing Collection ID.");
    }

    // Get Collection NID:
    $nid = nygb_collection_get_node_from_id($row->collection_id);
    if (!isset($nid) || !is_numeric($nid)) {
      throw new StructuredDataException("Invalid/Unparsed Collection ID: " . $row->collection_id);
    }
    $row->collection_nid = $nid;
    unset($data_keys['collection_id']);
    // Get Collection File FID (if available):
    if (isset($row->collection_file_id) && strlen($row->collection_file_id) || isset($row->file) && strlen($row->file)) {
      $collection_file_id = NULL;
      if (!isset($row->collection_file_id) && isset($row->file)) {
        $source = 'file';
        $collection_file_id = pathinfo($row->file, PATHINFO_FILENAME);
      }
      else {
        $source = 'collection_file_id';
        $collection_file_id = $row->collection_file_id;
      }

      $fid = nygb_collection_get_file_from_id($nid, $collection_file_id);
      if (isset($fid) && is_numeric($fid)) {
        $row->fid = $fid;
        unset($data_keys['collection_file_id']);
        unset($data_keys['file']);
        if (isset($row->image_number) && is_numeric($row->image_number)) {
          $row->page_number_index = $row->image_number;
          unset($data_keys['image_number']);
          // Get Pid from Page Entity
//          $pid = nygb_pdf_page_get_pid($fid, $row->pdf_page);
//          if (isset($pid) && is_numeric($pid)) {
//            $row->pid = $pid;
//            unset($data_keys['pdf_page']);
//          }
        }
      }
      else {
        $migration = Migration::currentMigration();
        $migration->queueMessage(t('Collection File ID: @file_id from @source was not detected in system, skipping row', array('@file_id' => $collection_file_id, '@source' => $source)));
        return FALSE;
      }
    }
    return $data_keys;
  }

  /**
   * Parse Header values to make them machine names.
   *
   * @param $headers
   *
   * @return mixed
   */
  static public function parseHeadersMigrate($headers) {
    $headerNames = $headers;
    $headerOutput = [];
    array_walk($headers, array('self', 'machinify_name'));
    foreach($headers as $key => $column_key) {
      $headerOutput[] = [$column_key, $headerNames[$key]];
    }
    return $headerOutput;
  }

  static protected function machinify_name(&$header_name, $key) {
    $header_name = str_replace(' ', '_', strtolower(filter_xss(trim($header_name))));
  }

  static protected function demachinify_name(&$header_name, $key) {
    $header_name = ucwords(str_replace('_', ' ', $header_name));
  }

  static public function recreateHumanHeaders($data) {
    $keys = array_keys($data);
    array_walk($keys, array('self', 'demachinify_name'));
    return array_combine($keys, $data);
  }

}

/**
 * Base class for any exceptions thrown in Data.
 */
class StructuredDataException extends Exception { }
