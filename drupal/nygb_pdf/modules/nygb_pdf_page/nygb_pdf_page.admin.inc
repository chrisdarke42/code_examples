<?php

/**
 * PDF Page Retrack Batch operation.
 */
function _nygb_pdf_page_file_retrack_batch_process($files, &$context) {
  if (!isset($context['sandbox']['progress'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = count($files);
    $context['sandbox']['files'] = $files;
  }

  // Process files by groups of 5.
  $count = min(5, count($context['sandbox']['files']));
  for ($i = 1; $i <= $count; $i++) {
    // For each fid, load the file, reset the values, and save it.
    $fid = array_shift($context['sandbox']['files']);
    $result = _nygb_pdf_page_set_pages($fid, FALSE, TRUE);
    if ($result === FALSE) {
      // Store result for post-processing in the finished callback.
      $context['errors'][] = $fid;
      drupal_set_message(t('PDF File %fid has no pages defined.', array('%fid' => $fid)));
      watchdog('NYGB Pdf Pages', 'PDF File %fid has no pages defined.', array('%fid' => $fid), WATCHDOG_WARNING);
    }
    else {
      // Store result for post-processing in the finished callback.
      $context['results'][] = t('Fid %fid has been processed correctly. %pages pages.', array('%fid' => $fid, '%pages' => $result));
      watchdog('NYGB Pdf Pages', 'Retracking %fid, %pages pages processed', array('%fid' => $fid, '%pages' => $result), WATCHDOG_INFO);
    }


    // Update our progress information.
    $context['sandbox']['progress']++;
  }

  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * PDF Page Retrack Batch 'finished' callback.
 */
function _nygb_pdf_page_file_retrack_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = format_plural(count($results), '1 item successfully processed:', '@count items successfully processed:');
    $message .= theme('item_list', array('items' => $results));
    drupal_set_message($message);
  }
  else {
    drupal_set_message(t('An error occurred and processing did not complete.'), 'error');
  }
}

/**
 * Menu callback.
 *
 * @see nygb_pdf_page_menu()
 */
function nygb_pdf_page_manage_files($form = array(), $form_state, $node) {
  $has_edit_access = node_access('update', $node);

  $indexable = _nygb_pdf_page_collection_indexable($node);

  $per_page = 20;
  $files = db_select('field_data_field_collection', 'fc')
    ->extend('PagerDefault')
    ->fields('fc', array('entity_id'))
    ->condition('fc.entity_type', 'file')
    ->condition('fc.bundle', 'collection_file')
    ->condition('fc.field_collection_target_id', $node->nid)
    ->orderBy('fc.entity_id')
    ->limit($per_page)
    ->execute()
    ->fetchAll();

  // If nothing found, just show a message.
  if (empty($files)) {
    drupal_set_message(t('No PDFs found for this collection.'));
    return $form;
  }

  // Display each file.
  if (!empty($files)) {
    $header = array(
      'filename' => t('Filename'),
      'field_count' => t('Number of Pages<br />(field data)'),
      'pdf_page_count' => t('Number of Pages<br />(database)'),
    );

    if ($has_edit_access) {
      $header['access'] = t('Access level');
    }
    if (user_access('access devel information')) {
      $header['operations'] = t('Operations');
    }

    $rows = [];
    // Add fields for each file.
    foreach ($files as $record) {
      $file = file_load($record->entity_id);
      // By default all files should require full "member" status in order to
      // access them.
      $member_role = user_role_load_by_name('Member');
      $file_access = $member_role->rid;
      if (!empty($file->field_file_access[LANGUAGE_NONE][0]['value'])) {
        if (is_numeric($file->field_file_access[LANGUAGE_NONE][0]['value'])) {
          $file_access = $file->field_file_access[LANGUAGE_NONE][0]['value'];
        }
      }

      $file_wrapper = entity_metadata_wrapper('file', $file->fid);
      $field_count = $file_wrapper->field_num_pages->value();

      if ($indexable) {
        $pdf_page_count = _nygb_pdf_page_get_page_count($file->fid);
        if ($field_count !== $pdf_page_count) {
          $pdf_page_count .= ' <strong>' . t("Numbers don't match!") . '</strong>';
        }
      }
      else {
        $page_count = _nygb_pdf_page_get_page_count($file->fid);
        $pdf_page_count = t('Image Browse only - not in index');
        if ($page_count > 0) {
          $pdf_page_count = t('Legacy PDF page data: %count. Needs deleting from index (Image Browse Only)', array('%count' => $page_count));
        }
      }

      $rows[$file->fid] = array(
        'filename' => l($file->filename, 'file/' . $file->fid),
        'field_count' => $field_count,
        'pdf_page_count' => $pdf_page_count,
      );
      if ($has_edit_access) {
        $rows[$file->fid]['access']['data'] = _nygb_pdf_page_manage_files_access_dropdown($file_access, $file->fid, NULL);
      }
      if (user_access('access devel information')) {
        $rows[$file->fid]['operations'] = implode(', ', array(
          l('Re-track PDF pages', '/node/' .  $node->nid . '/pdfs/retrack/' . $file->fid),
          l('Delete PDF Page Tracking', '/node/' .  $node->nid . '/pdfs/delete/' . $file->fid)
        ));
      }
    }
    $form['files'] = _nygb_pdf_page_manage_files_table($header, $rows);
  }

  // Pager.
  $count = db_select('field_data_field_collection', 'fc')
    ->fields('fc', array('entity_id'))
    ->condition('fc.entity_type', 'file')
    ->condition('fc.bundle', 'collection_file')
    ->condition('fc.field_collection_target_id', $node->nid)
    ->execute()
    ->rowCount();
  pager_default_initialize($count, $per_page);
  $form['pager'] = array('#theme' => 'pager', array('#quantity' => $count));

  // Submit button.
  $form['access_options'] = _nygb_pdf_page_manage_files_access_dropdown();
  $form['update_access'] = array(
    '#type' => 'submit',
    '#value' => t('Update Access'),
  );
  $form['retrack'] = array
  (
    '#type' => 'submit',
    '#value' => t('Re-track PDF Pages'),
  );
  $form['delete'] = array
  (
    '#type' => 'submit',
    '#value' => t('Delete PDF Page Tracking'),
  );
  $form['update_access_granular'] = array(
    '#type' => 'submit',
    '#value' => t('Update Access (Granular)'),
  );

  $form['#submit'][] = 'nygb_pdf_page_manage_files_submit';
  return $form;
}

/**
 * Generate dropdown for Access.
 *
 * @param int $file_access
 * @param null $fid
 * @param string $title
 *
 * @return array
 */
function _nygb_pdf_page_manage_files_access_dropdown($file_access = 31, $fid = NULL, $title = 'Access') {
  $name = 'access_update';
  if (isset($fid)) {
    $name = "access_selector[{$fid}]";
  }

  $options = [];
  if (!isset($file_access)) {
    $options[-1] = '--SELECT--';
  }

  $options[1] = 'Anonymous';
  $options[26] = 'Limited Access';
  $options[36] = 'Lapsed';
  $options[31] = 'Member';

  $dropdown =array(
    '#type' => 'select',
    '#options' => $options,
    '#value' => $file_access,
    '#name' => $name,
    '#title' => $title,
  );

  if (!isset($title)) {
    $dropdown['#title_display'] = 'invisible';
  }
  return $dropdown;
}

/**
 * FormAPI submission callback for nygb_pdf_page_manage_files().
 */
function nygb_pdf_page_manage_files_submit($form = array(), $form_state) {
  switch($form_state['triggering_element']['#parents'][0]) {
    case 'update_access_granular':
      // Look at the  ['input']['access_selector']
      if (isset($form_state['input']['access_selector'])) {
        _nygb_pdf_page_manage_files_set_granular_access($form_state['input']['access_selector']);
      }
      break;
    case 'retrack':
      // Look at the ['values']['files']
      if ($files = _nygb_pdf_page_manage_files_batch_select_valid($form_state)) {
        // Process Files
        nygb_pdf_page_file_retrack($files);
      }
      else {
        drupal_set_message('Please Select at least one File', 'error');
      }
      break;
    case 'delete':
      // Look at the ['values']['files']
      if ($files = _nygb_pdf_page_manage_files_batch_select_valid($form_state)) {
        // Process Files
        foreach($files as $fid) {
          _nygb_pdf_page_delete_all($fid);
        }
      }
      else {
        drupal_set_message('Please Select at least one File', 'error');
      }
      break;
    case 'update_access':
      // Look at the ['values']['files'] and ['input']['access_update']
      if ($files = _nygb_pdf_page_manage_files_batch_select_valid($form_state)) {
        // Process Files
        if (isset($form_state['input']['access_update']) && $form_state['input']['access_update'] > 0) {
          $files_access = [];
          foreach ($files as $fid) {
            $files_access[$fid] = $form_state['input']['access_update'];
          }
          _nygb_pdf_page_manage_files_set_granular_access($files_access);
        }
      }
      else {
        drupal_set_message('Please Select at least one File', 'error');
      }
      break;
  }

  if (!empty($form_state['values']['files']['rows'])) {
    foreach ($form_state['values']['files']['rows'] as $values) {
      if (isset($values['fid']) && is_numeric($values['fid'])) {
        // Load the corresponding file.
        $file = file_load($values['fid']);

        $updated = FALSE;
        // If the file had an access level defined already, updated it.
        if (isset($file->field_file_access[LANGUAGE_NONE][0]['value'])) {
          if ($file->field_file_access[LANGUAGE_NONE][0]['value'] != $values['access']) {
            $file->field_file_access[LANGUAGE_NONE][0]['value'] = $values['access'];
            $updated = TRUE;
          }
        }
        // If no access level assigned already, just save whatever value was
        // defined in the form.
        else {
          $file->field_file_access[LANGUAGE_NONE][0]['value'] = $values['access'];
          $updated = TRUE;
        }

        // If a change was made, update the file record.
        if ($updated) {
          file_save($file);
        }
      }
    }
  }
}

/**
 * Function for setting granular access on files.
 *
 * @param $access
 */
function _nygb_pdf_page_manage_files_set_granular_access($access) {
  foreach($access as $fid => $access_level) {
    _nygb_pdf_page_manage_files_update_access($fid, $access_level);
  }
}

/**
 * Function for running updates on files to set access.
 *
 * @param $fid
 * @param $access_level
 */
function _nygb_pdf_page_manage_files_update_access($fid, $access_level) {
  // Load the corresponding file.
  $file = file_load($fid);

  $updated = FALSE;
  // If the file had an access level defined already, updated it.
  if (isset($file->field_file_access[LANGUAGE_NONE][0]['value'])) {
    if ($file->field_file_access[LANGUAGE_NONE][0]['value'] != $access_level) {
      $file->field_file_access[LANGUAGE_NONE][0]['value'] = $access_level;
      $updated = TRUE;
    }
  }
  // If no access level assigned already, just save whatever value was
  // defined in the form.
  else {
    $file->field_file_access[LANGUAGE_NONE][0]['value'] = $access_level;
    $updated = TRUE;
  }

  // If a change was made, update the file record.
  if ($updated) {
    file_save($file);
  }
}

/**
 * Return selected files from tableselect.
 *
 * @param $form_state
 *
 * @return array|bool
 */
function _nygb_pdf_page_manage_files_batch_select_valid($form_state) {
  $return = FALSE;
  if (isset($form_state['values']['files'])) {
    $return = array_filter($form_state['values']['files']);
    $return = (count($return)) ? $return : FALSE;
  }
  return $return;
}

/**
 * Generate tableselect for the PDF Files table.
 */
function _nygb_pdf_page_manage_files_table($header, $rows) {
  $tableselect = array(
    '#type' => 'tableselect',
    '#header' => $header,
    '#options' => $rows,
  );
  return $tableselect;
}

/**
 * Run operations on pdf files.
 *
 * @param $node
 * @param $fid
 * @param $operation
 */
function nygb_pdf_page_pdf_operations($node, $fid, $operation) {
  switch($operation) {
    case 'retrack':
      nygb_pdf_page_file_retrack([$fid]);
      break;
    case 'delete':
      _nygb_pdf_page_delete_all($fid);
      break;
  }
  drupal_goto('node/' . $node->nid .'/pdfs');
}
