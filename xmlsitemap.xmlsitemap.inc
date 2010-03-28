<?php
// $Id$

/**
 * Implements hook_xmlsitemap_link_info().
 */
function xmlsitemap_xmlsitemap_link_info() {
  return array(
    'frontpage' => array(
      'label' => t('Frontpage'),
      'xmlsitemap' => array(
        'settings callback' => 'xmlsitemap_link_frontpage_settings',
      ),
    ),
  );
}

/**
 * XML sitemap link type settings callback for frontpage link entity.
 */
function xmlsitemap_link_frontpage_settings(&$form) {
  module_load_include('inc', 'xmlsitemap', 'xmlsitemap.admin');
  if (user_access('administer site configuration')) {
    $form['#description'] = t('The front page path can be changed at <a href="@url-frontpage">@url-frontpage</a>.', array('@url-frontpage' => url('admin/settings/site-information')));
  }
  $form['xmlsitemap_frontpage_priority'] = array(
    '#type' => 'select',
    '#title' => t('Priority'),
    '#options' => xmlsitemap_get_priority_options(),
    '#default_value' => variable_get('xmlsitemap_frontpage_priority', 1.0),
  );
  $form['xmlsitemap_frontpage_changefreq'] = array(
    '#type' => 'select',
    '#title' => t('Change frequency'),
    '#options' => xmlsitemap_get_changefreq_options(),
    '#default_value' => variable_get('xmlsitemap_frontpage_changefreq', XMLSITEMAP_FREQUENCY_DAILY),
  );
  return $form;
}

/**
 * Implements hook_xmlsitemap_link_alter().
 */
function xmlsitemap_xmlsitemap_link_alter(&$link) {
  // Alter the frontpage priority.
  if ($link['type'] == 'frontpage' || $link['loc'] == '' || $link['loc'] == drupal_get_normal_path(variable_get('site_frontpage', 'node'))) {
    $link['priority'] = xmlsitemap_var('frontpage_priority');
    $link['changefreq'] = xmlsitemap_var('frontpage_changefreq');
  }
}

/**
 * Implements hook_xmlsitemap_links().
 */
function xmlsitemap_xmlsitemap_links() {
  // Frontpage link.
  $links[] = array(
    'type' => 'frontpage',
    'id' => 0,
    'loc' => '',
  );
  return $links;
}

function xmlsitemap_xmlsitemap_sitemap_operations() {
  $operations['update'] = array(
    'label' => t('Update cached files'),
    'callback' => 'xmlsitemap_sitemap_multiple_update',
  );
  return $operations;
}

/**
 * Implements hook_xmlsitemap_context_url_options().
 */
function xmlsitemap_xmlsitemap_context_url_options(array $context) {
  $options['absolute'] = TRUE;
  return $options;
}

function _xmlsitemap_sitemap_context_summary(array $sitemap, $context_key, array $context_info) {
  $context_value = isset($sitemap['context'][$context_key]) ? $sitemap['context'][$context_key] : NULL;

  if (!isset($context_value)) {
    return t('Default');
  }
  elseif (!empty($context_info['summary callback'])) {
    return $context_info['summary callback']($context_value);
  }
  else {
    return $context_value;
  }
}
