<?php

class NygbPdfPageDataSourceController extends SearchApiEntityDataSourceController {

  /**
   * Entity type info for this type.
   *
   * @var array
   */
  protected $entityInfo;

  /**
   * The ID key of this entity type, if any.
   *
   * @var string|null
   */
  protected $idKey;

  /**
   * Returns information on the ID field for this controller's type.
   *
   * @return array
   *   An associative array containing the following keys:
   *   - key: The property key for the ID field, as used in the item wrapper.
   *   - type: The type of the ID field. Has to be one of the types from
   *     search_api_field_types(). List types ("list<*>") are not allowed.
   *
   * @throws SearchApiDataSourceException
   *   If any error state was encountered.
   */
  public function getIdFieldInfo() {
    return array(
      'key' => 'pid',
      'type' => 'string',
    );
  }

  /**
   * Load items of the type of this data source controller.
   *
   * Always returns an empty array. If you want the items of your type to be
   * loadable, specify a function here.
   *
   * @param array $ids
   *   The IDs of the items to load.
   *
   * @return array
   *   The loaded items, keyed by ID.
   */
  public function loadItems(array $ids) {
    $items = entity_load($this->entityType, $ids);
    // If some items couldn't be loaded, remove them from tracking.
    if (count($items) != count($ids)) {
      $ids = array_flip($ids);
      $unknown = array_keys(array_diff_key($ids, $items));
      if ($unknown) {
        search_api_track_item_delete($this->type, $unknown);
      }
    }

    return $items;
  }

  /**
   * Overrides SearchApiAbstractDataSourceController::getPropertyInfo().
   *
   * Only returns a single string ID field.
   */
  protected function getPropertyInfo() {
    $info['property info']['page_contents'] = array(
      'label' => t('Page Contents'),
      'type' => 'text',
      'term_vector' => 'with_positions_offsets',
    );
    return $info;
  }

  /**
   * {@inheritdoc}
   */
  public function startTracking(array $indexes) {
    if (!$this->table) {
      return;
    }
    // We first clear the tracking table for all indexes, so we can just insert
    // all items again without any key conflicts.
    $this->stopTracking($indexes);

    $table = $this->entityInfo['base table'];

    // We could also use a single insert (with a UNION in the nested query),
    // but this method will be mostly called with a single index, anyways.
    foreach ($indexes as $index) {
      // Select all entity ids.

      $query = db_select($table, 't');
      $query->addField('t', $this->idKey, 'item_id');
      $query->addExpression(':index_id', 'index_id', array(':index_id' => $index->id));
      $query->addExpression('1', 'changed');

      // INSERT ... SELECT ...
      db_insert($this->table)
        ->from($query)
        ->execute();
    }
  }

  /**
   * {@inheritdoc}
   */
  public function trackItemInsert(array $item_ids, array $indexes) {
    if (!$this->table || $item_ids === array()) {
      return;
    }

    foreach ($indexes as $index) {
      $this->checkIndex($index);
    }

    // Since large amounts of items can overstrain the database, only add items
    // in chunks.
    foreach (array_chunk($item_ids, 1000) as $chunk) {
      $insert = db_insert($this->table)
        ->fields(array($this->itemIdColumn, $this->indexIdColumn, $this->changedColumn));
      foreach ($chunk as $item_id) {
        foreach ($indexes as $index) {
          $insert->values(array(
            $this->itemIdColumn => $item_id,
            $this->indexIdColumn => $index->id,
            $this->changedColumn => 1,
          ));
        }
      }
      $insert->execute();
    }
  }

  /**
   * {@inheritdoc}
   */
  public function stopTracking(array $indexes) {
    if (!$this->table) {
      return;
    }
    // We could also use a single query with "IN" operator, but this method
    // will mostly be called with only one index.
    foreach ($indexes as $index) {
      $this->checkIndex($index);
      db_delete($this->table)
        ->condition($this->indexIdColumn, $index->id)
        ->execute();
    }
  }

  /**
   * {@inheritdoc}
   */
  public function getIndexStatus(SearchApiIndex $index) {
    if (!$this->table) {
      return array('indexed' => 0, 'total' => 0);
    }
    $this->checkIndex($index);
    $indexed = db_select($this->table, 'i')
      ->condition($this->indexIdColumn, $index->id)
      ->condition($this->changedColumn, 0)
      ->countQuery()
      ->execute()
      ->fetchField();
    $total = db_select($this->table, 'i')
      ->condition($this->indexIdColumn, $index->id)
      ->countQuery()
      ->execute()
      ->fetchField();
    return array('indexed' => $indexed, 'total' => $total);
  }
}

