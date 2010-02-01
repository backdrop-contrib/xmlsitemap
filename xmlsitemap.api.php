<?php
// $Id$

/**
 * @file
 * Hooks provided by the XML sitemap module.
 *
 * @ingroup xmlsitemap
 */

/**
 * @addtogroup hooks
 * @{
 */

/**
 * Provide information on the type of links this module provides.
 *
 * @see hook_entity_info()
 * @see hook_entity_info_alter()
 */
function hook_xmlsitemap_link_info() {
  return array(
    'mymodule' => array(
      'label' => 'My module',
      'base table' => 'mymodule',
      'object keys' => array(
        // Primary ID key on {base table}
        'id' => 'myid',
        // Subtype key on {base table}
        'bundle' => 'mysubtype',
      ),
      'path callback' => 'mymodule_path',
      'bundle label' => t('Subtype name'),
      'bundles' => array(
        'mysubtype1' => array(
          'label' => t('My subtype 1'),
          'admin' => array(
            'real path' => 'admin/settings/mymodule/mysubtype1/edit',
            'access arguments' => array('administer mymodule'),
          ),
          'xmlsitemap' => array(
            'status' => 1,
            'priority' => 0.5,
          ),
        ),
      ),
      'xmlsitemap' => array(
        // Callback function to take an array of IDs and save them as sitemap
        // links.
        'process callback' => '',
        // Callback function used in batch API for rebuilding all links.
        'rebuild callback' => '',
        // Callback function called from the XML sitemap settings page.
        'settings callback' => '',
      )
    ),
  );
}

/**
 * Alter the data of a sitemap link before the link is saved.
 *
 * @param $link
 *   An array with the data of the sitemap link.
 */
function hook_xmlsitemap_link_alter(&$link) {
  if ($link['type'] == 'mymodule') {
    $link['priority'] += 0.5;
  }
}

/**
 * @} End of "addtogroup hooks".
 */
