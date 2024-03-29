<?php
/**
 * @file
 * Hooks provided by the XML sitemap engines module.
 *
 * @ingroup xmlsitemap
 */

/**
 * @addtogroup hooks
 * @{
 */

/**
 * Provide a list of supported sitemap engines.
 */
function hook_xmlsitemap_engine_info() {
  $engines['example'] = array(
    'name' => t('Example search engine'),
    'url' => 'https://example.com/ping?sitemap=[sitemap]',
  );
  return $engines;
}

/**
 * Alter the list of sitemap engines.
 */
function hook_xmlsitemap_engine_info_alter(&$engines) {
  $engines['example']['name'] = t('Kitten Search');
  $engines['example']['url'] = 'https://kittens.com/ping?sitemap=[sitemap]';
}

/**
 * @} End of "addtogroup hooks".
 */
