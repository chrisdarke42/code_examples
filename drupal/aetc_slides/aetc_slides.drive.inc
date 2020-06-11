<?php

/**
 * @file
 * Google Drive functions for AETC Slides.
 */

require_once($_SERVER['DOCUMENT_ROOT'] . '/sites/all/libraries/vendor/autoload.php');

/**
 * Upload file to Google Drive.
 *
 * @param $file
 *
 * @return mixed
 */
function aetc_slides_drive_upload($file) {
  $type = _aetc_slides_identify_file_type($file->filemime);
  if (isset($type)) {
    dpm($file, 'aetc_slides_drive_upload');
    $documentId = _aetc_slides_drive_upload_document($file, $type);
    return $documentId;
  }
  else {
    watchdog(
      'AETC Slides',
      'Drive Upload Called but no valid mimetype for %filename (%fid) : %mimetype',
      array('%filename' => $file->uri, '%fid' => $file->uid, '%mimetype' => $file->filemime)
    );
  }
  return FALSE;
}

function aetc_slides_drive_delete($google_id) {
  try {
    $client = _aetc_slides_google_connect();
    $drive = new Google_Service_Drive($client);
    $drive->files->delete($google_id);
    return TRUE;
  } catch (Exception $e) {
    watchdog('AETC Slides', 'Google Drive Delete - ' . $e->getMessage());
    drupal_set_message('Drive Delete: ' . $e->getMessage());
    return FALSE;
  }
}

/**
 * Internal call to identify the type of a file (presentation, document).
 *
 * @param $filemime
 *
 * @return string|null
 */
function _aetc_slides_identify_file_type($filemime) {
  switch($filemime) {
    case 'application/vnd.openxmlformats-officedocument.presentationml.presentation':
    case 'application/vnd.ms-powerpoint':
      $type = 'presentation';
      break;
    case 'application/pdf':
      $type = 'document';
      break;
    default:
      $type = NULL;
  }
  return $type;
}

/**
 * Internal function to call the upload process to Google Drive.
 *
 * @param $file
 * @param $type
 *
 * @return mixed
 */
function _aetc_slides_drive_upload_document($file, $type) {
  $client = _aetc_slides_google_connect();
  $drive = new Google_Service_Drive($client);
  switch ($type) {
    case 'presentation':
      $mimetype = 'application/vnd.google-apps.presentation';
      break;
    default:
      $mimetype = 'application/vnd.google-apps.document';
  }
  $drive_file = new Google_Service_Drive_DriveFile(
    array(
      'name' => $file->filename,
      'mimeType' => $mimetype
    )
  );
  //  $file->setDescription('File DESCRIPTION IF AVAILABLE CAN BE SET HERE');

  try {
    dpm($file->uri, 'File URI for Upload');
    $filepath = drupal_realpath($file->uri);
    if ($data = file_get_contents($filepath)) {
      // Create File on Google Drive
      $createdFile = $drive->files->create($drive_file, array(
        'data' => $data,
        'mimeType' => $mimetype,
        'uploadType' => 'multipart'
      ));

      $documentId = $createdFile->id;

      // Create Permissions for Anyone to Read file on Google Drive
      $permission = new Google_Service_Drive_Permission();
      $permission->setRole('reader');
      $permission->setType('anyone');
      $drive->permissions->create($documentId, $permission);

      return $documentId;
    }
    else {
      drupal_set_message('File Contents not available: ' . $filepath);
    }
  }
  catch (Exception $e) {
    drupal_set_message('Upload Document: '. $e->getMessage());
  }
  return FALSE;
}

/**
 * Internal function to request a document with specific fields from Google API.
 *
 * @param $documentId
 * @param string $fields
 *
 * @return \Google_Service_Drive_DriveFile|Bool
 */
function _aetc_slides_get_document($documentId, $fields = '') {
  if (is_array($fields)) {
    $fields = implode(',', $fields);
  }
  $client = _aetc_slides_google_connect();
  $drive = new Google_Service_Drive($client);
  // "thumbnailLink,webViewLink,webContentLink,exportLinks"
  try {
    $file = $drive->files->get($documentId, array("fields"=>$fields));
    return $file;
  }
  catch (Exception $e) {
    watchdog('AETC Slides Drive', 'Get Document: '. $e->getMessage());
    return FALSE;
  }
}

/**
 * Internal function to connect to Google Drive Service.
 *
 * @return array|\Google_Client|mixed
 */
function _aetc_slides_google_connect() {
  try {
    $client = _aetc_slides_google_get_client();
    $client->setApplicationName("Google Drive Connection Test");
    $client->setScopes(Google_Service_Drive::DRIVE);
    return $client;
  }
  catch(Exception $e) {
    watchdog('AETC Slides Drive', 'Connect: ' . $e->getMessage());
  }
  return FALSE;
}

/**
 * Internal function get get Google Client object.
 *
 * @return array|\Google_Client|mixed
 */
function _aetc_slides_google_get_client() {
  $client = &drupal_static(__FUNCTION__);
  if (!isset($client)) {
    try {
      $client = new Google_Client();
      $client->useApplicationDefaultCredentials();
    }
    catch(Exception $e) {
      watchdog('AETC Slides', $e->getMessage());
    }
  }
  return $client;
}

/**
 * Request the Text/Plain version of the file and return the contents.
 *
 * @param $google_id
 *
 * @return false|string
 */
function aetc_slides_drive_get_text_copy($google_id) {
  return aetc_slides_drive_get_param_values($google_id, 'text');
}

function aetc_slides_drive_get_values($google_id, $params) {
  // If a string value is passed in as a param, convert to an array.
  $single_response = FALSE;
  if (!is_array($params)) {
    $single_response = TRUE;
    $params = [$params];
  }
  $fields = [];
  $callbacks = [];
  // Convert the simple text requests to the google drive API params and their
  // local parsing functions.
  foreach ($params as $param) {
    switch ($param) {
      case 'text':
        $fields[] = 'exportLinks';
        $callbacks[] = '_aetc_slides_get_text_content';
        break;
      case 'thumbnail':
        $fields[] = 'thumbnailLink, hasThumbnail';
        $callbacks[] = '_aetc_slides_get_thumbnail';
        break;
    }
  }
  // Request all the necessary fields from Google Drive API.
  $response = [];
  $google_file = _aetc_slides_get_document($google_id, $fields);
  // Parse the requested fields in the order they were requested.
  foreach($callbacks as $callback) {
    if ($google_file && function_exists($callback)) {
      $response[] = $callback($google_file, $google_id);
    }
    else {
      $response[] = FALSE;
    }
  }
  // If a single response was requested just pop that back.
  if ($single_response) {
    return array_pop($response);
  }

  return $response;
}

/**
 * Internal function to get the Text/Plain content version of a document.
 *
 * @param $google_file
 * @param $google_id
 *
 * @return bool|mixed
 */
function _aetc_slides_get_text_content($google_file, $google_id) {
  if (isset($google_file->exportLinks) && is_array($google_file->exportLinks)) {
    if (isset($google_file->exportLinks['text/plain'])) {
      $text_file_url = $google_file->exportLinks['text/plain'];
      if ($file_location = system_retrieve_file($text_file_url, file_directory_temp(), FALSE, FILE_EXISTS_REPLACE)) {
        return file_get_contents($file_location);
      }
    }
  }
  return FALSE;
}

/**
 * Internal function get the Thumbnail image of a document.
 *
 * @param $google_file
 * @param $google_id
 *
 * @return mixed
 */
function _aetc_slides_get_thumbnail($google_file, $google_id) {
  if ($google_file->hasThumbnail && isset($google_file->thumbnailLink) && strlen($google_file->thumbnailLink)) {
    $backup_url = 'https://drive.google.com/thumbnail?id=' . $google_id;
//    $client = _aetc_slides_google_connect();
//    $drive = new Google_Service_Drive($client);
//    $drive->files()->get()
    $directory = 'public://slides_thumbs';
    file_prepare_directory($directory, FILE_CREATE_DIRECTORY|FILE_MODIFY_PERMISSIONS);
    if ($file = system_retrieve_file($backup_url, 'public://slides_thumbs/' . $google_id . '.png', TRUE, FILE_EXISTS_REPLACE)) {
      return $file;
    }
  }
}

/**
 * Check if any fields of a Slide File that would be populated by Google are missing.
 *
 * @param $fid
 * @param null $file_wrapper
 *
 * @return array|bool
 */
function aetc_slides_file_params_check($fid, $file_wrapper = NULL) {
  if (!isset($file_wrapper)) {
    $file_wrapper = entity_metadata_wrapper('file', file_load($fid));
  }

  $google_id = $file_wrapper->field_google_drive_id->value();
  if (!isset($google_id)) {
    return FALSE;
  }
  $missing = [];
  $text = $file_wrapper->field_file_text->value();
  if (!isset($text) || !strlen($text)) {
    $missing[] = 'text';
  }
  if (!$file_wrapper->field_resource_thumbnail->value()) {
    $missing[] = 'thumbnail';
  }
  $params = ['google_id' => $google_id, 'missing' => $missing];
  return $params;
}

/**
 * Update any missing fields from those that would be populated by Google.
 *
 * @param $fid
 *
 * @return bool
 */
function aetc_slides_file_params_update($fid) {
  $file_wrapper = entity_metadata_wrapper('file', file_load($fid));
  $params = aetc_slides_file_params_check($fid, $file_wrapper);
  if (!$params) {
    return FALSE;
  }
  $values = array_combine($params['missing'], aetc_slides_drive_get_values($params['google_id'], $params['missing']));
  foreach($values as $key => $value) {
    if (!$value) {
      continue;
    }
    switch ($key) {
      case 'text':
        $file_wrapper->field_file_text->set($value);
        break;
      case 'thumbnail':
        $file_wrapper->field_resource_thumbnail->file->set($value);
        break;
    }
  }
  $file_wrapper->save();
}

/**
 * Request the Embed URL for a file that has a Google ID.
 *
 * @param $item
 *
 * @return string|null
 */
function aetc_slides_get_embed_url($item) {
  $type = _aetc_slides_identify_file_type($item['filemime']);
  if (isset($type)) {
    $file_wrapper = entity_metadata_wrapper('file', file_load($item['fid']));
    $google_id = $file_wrapper->field_google_drive_id->value();
    if (empty($google_id)) {
      return NULL;
    }
    if ($google_file = _aetc_slides_get_document($google_id, 'webViewLink')) {
      $start = variable_get('aetc_slides_autostart', FALSE) ? 'true' : 'false';
      $loop = variable_get('aetc_slides_loop', FALSE) ? 'true' : 'false';
      $delay = strval(variable_get('aetc_slides_delay', 10000));

      $webViewLink = $google_file->webViewLink;
      $baseWebLink = substr($webViewLink,0, strpos($webViewLink,'/edit?'));
      switch($type) {
        case 'presentation':
          return $baseWebLink . '/embed?start=' . $start . '&loop=' . $loop . '&delayms=' . $delay;
          break;
        default:
          return $baseWebLink . '/preview';
      }
    }
    else {
      drupal_set_message('File Embed missing');
    }
  }
  return NULL;
}

function aetc_slides_get_google_drive_info() {
  $client = _aetc_slides_google_connect();
  $drive = new Google_Service_Drive($client);
  $filesList = $drive->files->listFiles();
  $about = $drive->about->get(array('fields'=>'user'));
  return [$about['user'], $filesList['files']];
}
