<?php

/**
 * @file
 *
 * Admin functionality for AETC Slides and Google Integration.
 */

/**
 * AETC Slides Admin settings form.
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
function aetc_slides_admin_settings(array $form, array &$form_state) {
  module_load_include('inc', 'aetc_slides', 'aetc_slides.drive');
  list($user, $files) = aetc_slides_get_google_drive_info();
  $form['aetc_slides_header'] = array(
    '#markup' => '<p><strong>Google Cloud Details:</strong> ' . t('@username (%email)', array('@username' => $user->displayName, '%email' => $user->emailAddress)) . '</p>',
    '#weight' => -1,
  );

  $form['aetc_slides_files_summary'] = array(
    '#markup' => '<p><strong>Google Drive File Count:</strong> ' . count($files) . '</p>',
  );

  $form['aetc_slides_slide_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('Slide Embed Configuration'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );

  $form['aetc_slides_slide_settings']['aetc_slides_autostart'] = array(
    '#type' => 'checkbox',
    '#title' => t('Autostart slides'),
    '#default_value' => variable_get('aetc_slides_autostart', FALSE),
    '#description' => t("If checked, slides will default to autostarting animation."),
  );

  $form['aetc_slides_slide_settings']['aetc_slides_loop'] = array(
    '#type' => 'checkbox',
    '#title' => t('Loop slides'),
    '#default_value' => variable_get('aetc_slides_loop', FALSE),
    '#description' => t("If checked, slides will loop to the start when at the end."),
  );

  $form['aetc_slides_slide_settings']['aetc_slides_delay'] = array(
    '#type' => 'textfield',
    '#title' => t('Slide Delay.'),
    '#default_value' => variable_get('aetc_slides_delay', 10000),
    '#size' => 6,
    '#maxlength' => 6,
    '#description' => t("The delay, in milliseconds, between slides if autoplaying."),
  );

  return system_settings_form($form);
}
