<?php

/**
 * @file
 * PDF downloader for the NYGB PDF module.
 */

/**
 * PDF file download callback.
 *
 * Outputs filename of the downloadble file for the current request. If the file
 * doesn't exist, it is created.
 *
 * @param int $field_id
 *   The ID of the field to load.
 * @param int $page
 *   The page number to load.
 */
function nygb_pdf_get_file_path($field_id, $page) {
  $file = nygb_pdf_load_by_field_id($field_id);

  $file_path = nygb_pdf_generate_onepage_path($file, $page);

  print(file_create_url($file_path));
  exit();
}
