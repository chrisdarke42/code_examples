<?php

/**
 * @File
 *
 * AETC Slides Page functions.
 */

/**
 * Show information about what Google Drive has on a specific file.
 *
 * @param $file
 *
 * @return string
 */
function aetc_slides_file_google_drive($file) {
  $tmp = 'temporary://test.pdf';
  $filepath = drupal_realpath($tmp);
  dpm($filepath, $tmp);
  if (!$file) {
    watchdog('AETC Slides', 'Google Drive View - No File defined');
    return 'No File Defined';
  }

  module_load_include('inc', 'aetc_slides', 'aetc_slides.drive');
  $params = aetc_slides_file_params_check($file->fid);
  if (!$params) {
    watchdog('AETC Slides', 'Google Drive View - No Google ID');
    return 'No Google ID defined';
  }

  $header = array(t('Google Parameter'), t('Value'));
  $google_params = [
    'id',
    'webViewLink',
    'webContentLink',
    'hasThumbnail',
    'thumbnailLink',
    'exportLinks',
  ];
  try {
    if ($raw_data = _aetc_slides_get_document($params['google_id'], $google_params)) {
      $rows = [];
      foreach ($google_params as $param) {
        $row = [];
        $row[] = $param;
        $value = 'Not Available';
        if (isset($raw_data->$param)) {
          $param_data = $raw_data->$param;

          if (is_array($param_data)) {
            array_walk($param_data, function (&$value, $key) {
              $value = '<strong>'.$key . '</strong>: ' . $value;
            });
            $value = implode('<br/>', $param_data);
          }
          else {
            $value = $param_data;
          }
        }
        $row[] = $value;
        $rows[] = $row;
      }
      $tag = theme_html_tag(['element' => ['#tag'=>'img', '#attributes' => ['src' => 'https://drive.google.com/thumbnail?id=' . $params['google_id']]]]);
      $rows[] = ['Rendered Thumbnail', $tag];

      if (count($params['missing'])) {
        $rows[] = [t('Rebuild Missing Data from Google Drive (%fields):', array('%fields' => implode(', ', $params['missing'])) ), l('Click Here to Rebuild', '/file/' . $file->fid . '/gdrive_update')];
      }

      $build['google_drive_table'] = array(
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $rows,
        '#caption' => t('This table shows all the pertinent google drive data for @filename.', array('@filename' => $file->filename)),
        '#empty' => t('There is no google drive data available.'),
      );
      return $build;
    }
    else {
      drupal_set_message('File Embed Missing');
      return 'No Data available for the documentId: ' . $params['google_id'];
    }
  } catch (Exception $e) {
    watchdog('AETC Slides', 'Google Drive View: ' . $e->getMessage());
    drupal_set_message($e->getMessage());
    return 'Error from Google Drive';
  }
}

/**
 * Form for the 'Update from Google Drive' button for aetc_slides_file_google_drive().
 *
 * @param $form
 * @param $form_state
 * @param $file
 *
 * @return bool|mixed
 */
function aetc_slides_file_google_drive_update($form, &$form_state, $file) {
  if (!$file) {
    return FALSE;
  }
  $file_wrapper = entity_metadata_wrapper('file', $file);
  $google_id = $file_wrapper->field_google_drive_id->value();
  if (!isset($google_id)) {
    return FALSE;
  }

  $form_state['file'] = $file;

  $form['fid'] = array(
    '#type' => 'value',
    '#value' => $file->fid,
  );

  $header = array(t('Google Parameter'), t('Value'));
  $google_params = [
    'id',
    'webViewLink',
    'webContentLink',
    'hasThumbnail',
    'thumbnailLink',
    'exportLinks',
  ];

  $description = t('This will set Thumbnail and Text values.');
  //  if ($references = file_usage_list($file)) {
  //    $description .= ' ' . t('This file is currently in use and may cause problems if deleted.');
  //  }

  return confirm_form($form,
    t('Are you sure you want to fetch missing attributes for %title from Google Drive?', array(
      '%title' => entity_label('file', $file),
    )),
    'file/' . $file->fid,
    $description,
    t('Update from Google Drive')
  );
}

/**
 * Form submission handler for aetc_slides_file_google_drive_update().
 */
function aetc_slides_file_google_drive_update_submit($form, &$form_state) {
  if ($form_state['values']['confirm'] && $file = file_load($form_state['values']['fid'])) {

    $args = array(
      '@type' => file_entity_type_get_name($file),
      '%title' => entity_label('file', $file),
    );
    module_load_include('inc', 'aetc_slides', 'aetc_slides.drive');
    aetc_slides_file_params_update($file->fid);

    watchdog('file', '@type: updated %title from Google Drive.', $args);
    drupal_set_message(t('@type %title has been updated.', $args));
  }

  $form_state['redirect'] = '/file/' . $file->fid . '/view';

  // Clear the page and block caches.
  cache_clear_all();
}

/**
 * Get Resource Slide File.
 * For when a Resource file is called by its resource id.
 */
function aetc_slides_get_resource_file($resource_title, $file_id) {
  if (is_numeric($file_id)) {
    $alias_format = 'resource/%s';
    $alias = sprintf($alias_format, check_plain($resource_title));
    if ($alias) {
      $path = drupal_get_normal_path($alias);
      $item = menu_get_item($path);
      if ($node = $item['page_arguments'][0]) {
        $node_wrapper = entity_metadata_wrapper('node', $node);
        if (isset($node_wrapper->field_media_files) && isset($node_wrapper->field_media_files[$file_id])) {
          $file_data = $node_wrapper->field_media_files[$file_id]->value();
          if (isset($file_data['fid'])) {
            $render_array = file_view(file_load($file_data['fid']));
            $page_title = t('Slide @counter from <a href="@url">@node_title</a> page', array(
              '@counter' => $file_id,
              '@url' => url($alias),
              '@node_title' => $node->title,
            ));
            $render_array['node_info'] = [
              '#markup' => '<h2>' . $page_title . '</h2>',
              '#weight' => -100,
            ];
            return $render_array;
          }
        }
      }
    }
  }
  return drupal_not_found();
}
