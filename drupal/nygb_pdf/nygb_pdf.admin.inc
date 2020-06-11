<?php

/**
 * @file
 * NYGB PDF Settings Administration.
 */

/**
 * NYGB PDF Admin settings form.
 *
 * @param array $form
 *   Form Array.
 * @param array $form_state
 *   Form State Array.
 *
 * @return mixed
 *   The admin form.
 *
 * @throws \Exception
 */
function nygb_pdf_admin_settings(array $form, array &$form_state) {
  $url = $GLOBALS['base_url'] . '/api/v1/NPcopyright.json';
  $form['nygb_pdf_copyright_info'] = array(
    '#type' => 'textfield',
    '#title' => t('PDF Copyright Info.'),
    '#description' => t('The text data that will appear at the bottom of the PDFs presented to the users. Endpoint for this is %url', array('%url' => $url)),
    '#default_value' => variable_get('nygb_pdf_copyright_info', ''),
    '#size' => 100,
    '#maxlength' => 1000,
    '#weight' => 1,
    '#required' => TRUE,
  );

  return system_settings_form($form);
}
