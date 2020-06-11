<?php


/**
 * Structured Data Delete Batch operation.
 */
function _nygb_structured_data_delete_batch_process($sids, &$context) {
  if (!isset($context['sandbox']['progress'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = count($sids);
    $context['sandbox']['sids'] = $sids;
  }

  // Process files by groups of 10.
  $count = min(1000, count($context['sandbox']['sids']));
  $sids = [];
  for ($i = 1; $i <= $count; $i++) {
    // For each sid, call the entity_delete function.
    $data = array_shift($context['sandbox']['sids']);
    $sid = $data->sid;
    $sids[] = $sid;
  }
  $result = entity_delete_multiple('structured_data', $sids);
  if ($result === FALSE) {
    // Store result for post-processing in the finished callback.
    drupal_set_message(t('Error Deleting Structured Data.'));
    watchdog('NYGB Structured Data', 'Error Deleting Structured Data.', WATCHDOG_WARNING);
  }
  else {
    // Update our progress information.
    $context['sandbox']['progress'] = $context['sandbox']['progress'] + $count;
    $context['results'][] = t('%count Structured Data Entries have been deleted.', array('%count' => $count));
    watchdog('NYGB Pdf Pages', '%count Structured Data Entries have been deleted.', array('%count' => $count, WATCHDOG_INFO));
  }

  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * Structured Data Delete Batch 'finished' callback.
 */
function _nygb_structured_data_delete_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = format_plural(count($results), '1 item successfully processed:', '@count items successfully processed:');
    $message .= theme('item_list', array('items' => $results));
    drupal_set_message($message);
  }
  else {
    drupal_set_message(t('An error occurred and processing did not complete.'), 'error');
  }
}

function _nygb_structured_data_info($node) {
  $structured_data_info = _nygb_structured_data_get_data_for_collection($node);
  $rows = [];
  $header = [];
  foreach($structured_data_info as $data) {
    $header = ['Related File', 'Number of Entries', 'Operations'];
    $label = 'No Associated File';
    $fid = 'none';
    if (isset($data->fid)) {
      if (is_numeric($data->fid) && $data->fid > 0) {
        $fid = $data->fid;
        $file_wrapper = entity_metadata_wrapper('file', $data->fid);
        if (isset($file_wrapper)) {
          $label = $file_wrapper->label();
        }
        else {
          $label = t('FID (%fid) not available', array('%fid' => $fid));
        }
      }
      else {
        if ($data->fid == 0) {
          $fid = 'zero';
        }
      }
    }

    $operations = l('[ Delete Structured Data - Re-Migrate required to rebuild ]', '/node/' .  $node->nid . '/structured/delete/' . $fid);
    $rows[] = [$label, $data->count, $operations];
  }
  $attributes = [];
  $variables = array('header' => $header, 'rows' => $rows, 'attributes' => $attributes, 'sticky' => FALSE, 'caption' => NULL, 'colgroups' => array(), 'empty' => 'No Structured data stored for this Collection.');
  return theme('table', $variables);
}

function _nygb_structured_data_operations($node, $fid, $operation) {
  switch($operation) {
    case 'delete':
      $entries = _nygb_structured_data_get_entries($node, $fid);
      $batch = array(
        'operations' => array(
          array(
            '_nygb_structured_data_delete_batch_process',
            array($entries),
          ),
        ),
        'finished' => '_nygb_structured_data_delete_batch_finished',
        'title' => t('Processing'),
        // We use a single multi-pass operation, so the default
        // 'Remaining x of y operations' message will be confusing here.
        'progress_message' => '',
        'error_message' => t('The update has encountered an error.'),
        // The operations do not live in the .module file, so we need to
        // tell the batch engine which file to load before calling them.
        'file' => drupal_get_path('module', 'nygb_structured_data') . '/nygb_structured_data.admin.inc',
      );
      batch_set($batch);
      batch_process('node/' . $node->nid .'/structured');
      break;
  }
}
