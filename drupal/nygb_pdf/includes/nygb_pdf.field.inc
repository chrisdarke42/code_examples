<?php

/**
 * Implements hook_field_formatter_info().
 */
function nygb_pdf_field_formatter_info() {
  return array(
    'pdf_nygb' => array(
      'label' => t('PDF: NYGB PDF version'),
      'description' => 'PDF: NYGB PDF version of PDF.js.',
      'field types' => array('file'),
      'settings' => array(
        'load_on_view' => TRUE,
        'keep_pdfjs' => TRUE,
        'width' => '100%',
        'height' => '600px',
        'page' => '',
      ),
    ),
    'pdf_nygb_single_page' => array(
      'label' => t('PDF: NYGB Single Page PDF'),
      'description' => 'PDF: NYGB Single Page PDF version of PDF.js.',
      'field types' => array('file'),
      'settings' => array(
        'keep_pdfjs' => TRUE,
        'width' => '100%',
        'height' => '600px',
        'page' => '',
      ),
    ),
  );
}

/**
 * Implements hook_field_formatter_settings_form().
 */
function nygb_pdf_field_formatter_settings_form($field, $instance, $view_mode, $form, &$form_state) {
  $display = $instance['display'][$view_mode];
  $settings = $display['settings'];
  $element = array();
  switch ($display['type']) {
    case 'pdf_nygb':
    case 'pdf_nygb_single_page':
      $element['page'] = array(
        '#type' => 'textfield',
        '#title' => 'Page',
        '#default_value' => $settings['page'],
        '#description' => t('Page number to display. In the absence of a value, first page will be displayed. If the number is too high, last page will be displayed.'),
      );
      $element['load_on_view'] = array(
        '#type' => 'checkbox',
        '#title' => t('Only load on View'),
        '#default_value' => $settings['load_on_view'],
        '#description' => t("Only actually load file when the PDF is in the viewport display. (This saves on loading extra content from S3)"),
      );
      $element['keep_pdfjs'] = array(
        '#type' => 'checkbox',
        '#title' => t('Always use pdf.js'),
        '#default_value' => $settings['keep_pdfjs'],
        '#description' => t("Use pdf.js even the browser has Adobe Reader Plugin, WebKit PDF Reader for Safari or the PDF Reader for Chrome (Chrome's default alternative to the Adobe Reader Plugin) installed."),
      );
      $element['width'] = array(
        '#type' => 'textfield',
        '#title' => 'Width',
        '#default_value' => $settings['width'],
        '#description' => t('Width of the viewer. Ex: 250px or 100%'),
      );
      $element['height'] = array(
        '#type' => 'textfield',
        '#title' => 'Height',
        '#default_value' => $settings['height'],
        '#description' => t('Height of the viewer. Ex: 250px or 100%'),
      );
  }
  return $element;
}

/**
 * Implements hook_field_formatter_settings_summary().
 */
function nygb_pdf_field_formatter_settings_summary($field, $instance, $view_mode) {
  $display = $instance['display'][$view_mode];
  $settings = $display['settings'];
  if ($display['type'] === 'pdf_nygb') {
    return t('Use pdf.js even users have PDF reader plugin: @keep_pdfjs', array('@keep_pdfjs' => $settings['keep_pdfjs'] ? t('Yes') : t('No'))) . '<br />' . t('Width: @width , Height: @height', array('@width' => $settings['width'] , '@height' => $settings['height']));
  }
  if ($display['type'] === 'pdf_nygb_single_page') {
    return t('Show page @page_value', array('@page_value' => $settings['page'])) . '<br/>' .
      t('Use pdf.js even users have PDF reader plugin: @keep_pdfjs', array('@keep_pdfjs' => $settings['keep_pdfjs'] ? t('Yes') : t('No'))) . '<br />' .
      t('Width: @width , Height: @height', array('@width' => $settings['width'] , '@height' => $settings['height']));
  }
  return '';
}

/**
 * Implements hook_field_formatter_view().
 */
function nygb_pdf_field_formatter_view($entity_type, $entity, $field, $instance, $langcode, $items, $display) {
  $element = array();
  $settings = $display['settings'];
  switch ($display['type']) {
    case 'pdf_nygb':
      foreach ($items as $delta => $item) {
        if (strpos($item['filemime'], 'pdf')) {
          $element[$delta] = array(
            '#theme' => 'nygb_pdf_formatter',
            '#file' => (object) $item,
            '#load_on_view' => $settings['load_on_view'],
            '#keep_pdfjs' => $settings['keep_pdfjs'],
            '#width'  => $settings['width'],
            '#page' => $settings['page'],
            '#height'  => $settings['height'],
          );
        }
      }
      break;
    case 'pdf_nygb_single_page' :
      foreach ($items as $delta => $item) {
        if (strpos($item['filemime'], 'pdf')) {
          $element[$delta] = array(
            '#theme' => 'nygb_pdf_formatter',
            '#file' => (object) $item,
            '#keep_pdfjs' => $settings['keep_pdfjs'],
            '#width'  => $settings['width'],
            '#height'  => $settings['height'],
            '#page' => $settings['page'],
            '#singlepage' => TRUE,
          );
        }
      }
      break;
  }
  return $element;
}

/**
 * Theme function for NYGB PDF formatter.
 */
function theme_nygb_pdf_formatter($variables) {
  drupal_add_js(drupal_get_path('module', 'nygb_pdf') . '/js/nygb_pdf_iframe.js');
  $library = libraries_load('pdf.js');
  if ($library['loaded'] == FALSE) {
    drupal_set_message($library['error message'], 'error');
    return 'Please download and install ' . l($library['name'], $library['download url']) . '!';
  }

  $error = FALSE;

  $pagenum = NULL;
  // Get the page number from the settings.
  if (isset($variables['page'])) {
    $pagenum = $variables['page'];
  }

  // Get the page number from the end of the URL path.
  $path_page = _nygb_pdf_get_page_from_url();
  if ($path_page) {
    $pagenum = $path_page;
  }

  $ajax = FALSE;
  $single_page = FALSE;
  $file_id = $variables['file']->fid;

  // Check if the output is singlepage
  if (!empty($variables['singlepage']) && $variables['singlepage']) {
    if(is_numeric($pagenum)) {
      $single_page = TRUE;
    }
  }


  $force_pdfjs = $variables['keep_pdfjs'];
  // Initialize attributes.
  $pages = _nygb_pdf_get_page_count($variables['file']);
  $attributes = [
    'class' => ['nygb-load_pdf_callback', 'pdf'],
    'width' => $variables['width'],
    'height' => $variables['height'],
    'data-pagecount' => $pages,
    'data-docsource' => $GLOBALS['base_url'] . '/pdfs/' . $file_id,
    'data-page' => $pagenum,
  ];

  // If its a multi page and and load on view is selected:
  if (!$single_page) {
    $display = _nygb_pdf_display_full($file_id);
    if (!$display) {
      $markup = theme('html_tag', array(
        'element' => array(
          '#tag' => 'h2',
          '#attributes' => array(
            'class' => '',
          ),
          '#value' => t('View Page Not Available'),
        ),
      ));
      $html = [
        '#markup' => $markup,
      ];
      return render($html);
    }
    if (isset($variables['load_on_view']) && $variables['load_on_view']) {
      $attributes['class'][] = 'nygb-load_on_view';
      $ajax = TRUE;
    }
  }
  // Else if it's marked for ajax.
  elseif ($single_page) {
    $attributes['class'][] = 'nygb-load_singe_page';
  }
  if ($ajax) {
    // Ajax loaded implementation.
    $attributes['fid'] = $file_id;
    $attributes['page'] = $pagenum;
    $html = _nygb_pdf_iframe_placeholder($attributes);
  }
  else {
    // Basic implementation is to load the iframe.
    $url = _nygb_pdf_generate_url($file_id, $single_page, $pagenum);
    if (!$url) {
      // Here we are rendering the pdf immediately. Throw access denied if no access.
      $html = nygb_pdf_get_pdf_iframe_error(403, FALSE);
      $error = TRUE;
    }
    else {
      $attributes['data-src'] = $url;
      // @todo mask this url with something else?
      $attributes['src'] = _nygb_pdf_generate_source_url($url);

      // Determine whether to pass in the "data-downloadable" tag; if this
      // option is enabled the "download" and "print" buttons will be displayed,
      // otherwise they will not be.
      if (nygb_collection_is_downloadable($file_id)) {
        $attributes['data-downloadable'] = TRUE;
      }
      $html = _nygb_pdf_iframe($attributes);
    }

  }

  if ($force_pdfjs != TRUE && !$error) {
    $module_path = drupal_get_path('module', 'pdf');
    drupal_add_js($module_path . '/js/acrobat_detection.js');
    drupal_add_js($module_path . '/js/default.js');
  }
  return render($html);
}

/**
 * Create iframe render array
 *
 * @param array $data
 *   Relevant data array.
 *
 * @return array
 *   Render array.
 */
function _nygb_pdf_iframe($data) {
  $drupal_attributes = [
    'class' => '',
    'width' => '',
    'height' => '',
    'src' => '',
    'data-src' => '',
    'data-pagecount' => '',
    'data-docsource' => '',
    'data-page' => '',
    'data-downloadable' => '',
    ];

  $extra_attributes = array_intersect_key($data, $drupal_attributes);

  $attributes = array(
    'webkitallowfullscreen' => '',
    'mozallowfullscreen' => '',
    'allowfullscreen' => '',
    'frameborder' => 'no',
  );
  $attributes = array_merge($attributes, $extra_attributes);
  $html = array(
    '#class' => '',
    '#type' => 'html_tag',
    '#tag' => 'iframe',
    '#value' => 'PDF iFrame',
    '#attributes' => $attributes,
  );
  return $html;
}

/**
 * Create iframe placeholder
 *
 * @param array $data
 *   Relevant data array.
 *
 * @return string
 *   Themed html.
 */
function _nygb_pdf_iframe_placeholder(array $data) {
  foreach($data as $key => $value) {
    if ($key !== 'class') {
      $data['data-' . $key] = $value;
      unset($data[$key]);
    }
  }
  $html = theme('html_tag', array(
    'element' => array(
      '#tag' => 'div',
      '#attributes' => $data,
    ),
  ));
  return $html;
}
