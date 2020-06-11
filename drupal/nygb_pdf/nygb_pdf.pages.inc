<?php

/**
 * @file
 * Menu callbacks for the NYGB PDF module.
 */

/**
 * PDF file path callback.
 *
 * Outputs path of the downloadble file for the current request. This ends up
 * being the internal path 'pdfs/%/%' as we use a wrapper for the S3 storage.
 *
 * @param int $file_id
 *   The file's ID.
 * @param int $page
 *   The page number to load.
 */
function nygb_pdf_get_file_path($file_id, $page) {
  print url('pdfs/' . $file_id . '/' . $page, array('absolute' => TRUE));
  exit();
}

/**
 * Menu callback for transmitting a PDF from S3 to the browser.
 *
 * @param int $file_id
 *   The file's ID.
 * @param int|bool $page
 *   The page number to load; defaults to FALSE, which loads the whole file.
 * @param bool $inline
 *   Controls whether the file will be transmitted to the browser inline or as
 *   a file download (attachment). Defaults to TRUE.
 */
function nygb_pdf_transmit_pdf($file_id, $page = FALSE, $inline = TRUE) {
  // Identify whether the file is to be transferred inline or as an attachment.
  if ($inline)
    $transfer_method = 'inline';
  else {
    $transfer_method = 'attachment';
  }

  $error = FALSE;

  // Load the file object.
  $file = file_load($file_id);
  _nygb_pdf_get_pdf_watchdog($file, $page);
  if (!empty($file)) {
    // If a specific page number was requested, generate that path.
    if (is_numeric($page)) {

      // Work out what the single page filename & path will be.
      $page_data = _nygb_s3_pdf_single_page($file, $page);
      if ($page_data !== FALSE && !isset($page_data['error'])) {
        // Generate a fake file object that can be transmitted to the visitor.
        $file = new StdClass();
        $file->uri = $page_data['uri'];
        $file->filesize = filesize($page_data['uri']);
        $file->filename = $page_data['filename'];
      }
      else {
        $error = TRUE;
        if (!$page_data) {
          watchdog(
            'NYGB PDF',
            'File data for File:%fid page:%page with URI source %uri could not be acquired.',
            array('%fid' => $file_id, '%page' => $page, '%uri' => $file->uri),
            WATCHDOG_ALERT);
          drupal_not_found();
        }
        else {
          watchdog(
            'NYGB PDF',
            'File data for File:%fid page:%page with destination URI %uri could not be acquired.',
            array('%fid' => $file_id, '%page' => $page, '%uri' => $page_data['uri']),
            WATCHDOG_ALERT);
          drupal_not_found();
        }

      }
    }

    if (!$error) {
      // Transmit the file to the visitor.
      nygb_pdf_transmit_file($file, $transfer_method);
    }
  }
  else {
    drupal_not_found();
  }

  drupal_exit();
}

/**
 * Build out iframe for AJAX request to render on page when tab is visible.
 */
function nygb_pdf_get_pdf_iframe() {
  $data = $_POST;
  $single = FALSE;
  $url = FALSE;
  if (isset($_POST['type']) && $_POST['type'] === 'single') {
    $single = TRUE;
  }
  if (isset($data['fid'])) {
    $fid = $data['fid'];
    $page = NULL;
    if (isset($data['page'])) {
      $page = $data['page'];
    }
    $url = _nygb_pdf_generate_url($fid, $single, $page);
  }
  if ($url) {
    $data['data-src'] = $url;
    $data['src'] = _nygb_pdf_generate_source_url($url);
    $html = _nygb_pdf_iframe($data, TRUE);
    print(render($html));
  }
  exit();
}

/**
 * Get Collection File callback.
 * For when a collection file is called with a page number on the end.
 *
 * @param string $collection
 *   Collection Name.
 * @param string $file_id
 *   File id.
 * @param int $page
 *   Page Number.
 *
 * @return mixed
 *   Rendered Page.
 */
function nygb_pdf_get_collection_file($collection, $file_id, $page) {
  $alias_format = 'online-records/%s/%s';
  $alias = sprintf($alias_format, $collection, $file_id);
  $path = drupal_get_normal_path($alias);
  preg_match('/file\/(\d*)/', $path, $path_parts);
  if (count($path_parts) > 1 && is_numeric($path_parts[1])) {
    $file_id = $path_parts[1];
    if (!nygb_pdf_pdf_access($file_id, $page)) {
      return MENU_ACCESS_DENIED;
    }
    $file_entity = file_load($file_id);
    if (isset($file_entity)) {
      $file_render_array = file_view($file_entity, 'single_page');
      return $file_render_array;
    }
  }
}

function _nygb_pdf_get_pdf_watchdog($file, $page = NULL) {
  $page_view = 'Full';
  if ($page) {
    $page_view = t('Page @page', array('@page' => $page));
  }
  $watchdog_data = json_encode(['s3_source' => $file->uri, 'fid' => $file->fid, 'view' => $page_view]);
  watchdog('nygb pdf view', $watchdog_data);
}
