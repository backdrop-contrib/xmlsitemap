<?php
// $Id$

/**
 * @file
 * Sitemap generation and rebuilding functions for the xmlsitemap module.
 *
 * @ingroup xmlsitemap
 */

/**
 * Extended class for writing XML sitemap files.
 */
class XMLSitemapWriter extends XMLWriter {
  protected $status = TRUE;
  protected $uri = NULL;
  protected $sitemapElementCount = 0;
  protected $linkCountFlush = 500;

  public function openURI($uri) {
    $return = parent::openUri($uri);
    if ($return) {
      $this->uri = $uri;
      $this->setIndent(FALSE);
      $this->startDocument('1.0', 'UTF-8');
    }
    return $return;
  }

  /**
   * Write an full XML sitemap element tag.
   *
   * @param $name
   *   The element name.
   * @param $element
   *   An array of the elements properties and values.
   * @param $sitemap
   *   The XML sitemap array.
   */
  public function writeSitemapElement($name, array &$element, array $sitemap) {
    $this->startElement($name);
    foreach ($element as $key => $value) {
      $this->writeElement($key, $value);
    }
    $this->endElement();
    $this->writeRaw(PHP_EOL);

    // After a certain number of elements have been added, flush the buffer
    // to the output file.
    $this->sitemapElementCount++;
    if (($this->sitemapElementCount % $this->linkCountFlush) == 0) {
      $this->flush();
    }
  }

  /**
   * Override of XMLWriter::flush() to track file writing status.
   */
  public function flush($empty = TRUE) {
    $return = parent::flush($empty);
    $this->status &= (bool) $return;
    return $return;
  }

  public function getStatus() {
    return $this->status;
  }

  public function getURI() {
    return $this->uri;
  }

  public function getSitemapElementCount() {
    return $this->sitemapElementCount;
  }
}

/**
 * Given an internal Drupal path, return the alias for the path.
 *
 * This is similar to drupal_get_path_alias(), but designed to fetch all alises
 * at once so that only one database query is executed instead of several or
 * possibly thousands during sitemap generation.
 *
 * @param $path
 *   An internal Drupal path.
 * @param $language
 *   A language code to look use when looking up the paths.
 */
function xmlsitemap_get_path_alias($path, $language) {
  static $aliases;
  static $last_language;

  if (!isset($aliases)) {
    $aliases[LANGUAGE_NONE] = db_query("SELECT source, alias FROM {url_alias} WHERE language = :language ORDER BY pid", array(':language' => LANGUAGE_NONE))->fetchAllKeyed();
  }
  if ($language != LANGUAGE_NONE && $last_language != $language) {
    unset($aliases[$last_language]);
    $aliases[$language] = db_query("SELECT source, alias FROM {url_alias} WHERE language = :language ORDER BY pid", array(':language' => $language))->fetchAllKeyed();
    $last_language = $language;
  }

  if ($language != LANGUAGE_NONE && isset($aliases[$language][$path])) {
    return $aliases[$language][$path];
  }
  elseif (isset($aliases[LANGUAGE_NONE][$path])) {
    return $aliases[LANGUAGE_NONE][$path];
  }
  else {
    return $path;
  }
}

/**
 * Perform operations before rebuilding the sitemap.
 */
function _xmlsitemap_regenerate_before() {
  // Attempt to increase the available processing time and memory limit.
  drupal_set_time_limit(240);
  _xmlsitemap_set_memory_limit();

  // Set a timer so we can track how long this takes.
  timer_start('xmlsitemap_regenerate');

  if (variable_get('xmlsitemap_developer_mode', 0)) {
    watchdog('xmlsitemap', 'Starting XML sitemap generation. Memory usage: @memory-peak.', array(
        '@memory-peak' => format_size(memory_get_peak_usage(TRUE)),
      ),
      WATCHDOG_DEBUG
    );
  }

  // Clear the maximum chunk and file size variables.
  variable_set('xmlsitemap_max_chunks', 0);
  variable_set('xmlsitemap_max_filesize', 0);
}

function _xmlsitemap_get_memory_usage($start = FALSE) {
  static $memory_start;
  $current = memory_get_peak_usage(TRUE);
  if (!isset($memory_start) || $start) {
    $memory_start = $current;
  }
  return $current - $memory_start;
}

/**
 * Calculate the optimal PHP memory limit for sitemap generation.
 *
 * This function just makes a guess. It does not take into account
 * the currently loaded modules.
 */
function _xmlsitemap_get_optimal_memory_limit() {
  $optimal_limit = &drupal_static(__FUNCTION__);
  if (!isset($optimal_limit)) {
    // Set the base memory amount from the provided core constant.
    $optimal_limit = parse_size(DRUPAL_MINIMUM_PHP_MEMORY_LIMIT);

    // Add memory based on the chunk size.
    $optimal_limit += xmlsitemap_get_chunk_size() * 500;

    // Add memory for storing the url aliases.
    if (variable_get('xmlsitemap_prefetch_aliases', 1)) {
      $aliases = db_query("SELECT COUNT(pid) FROM {url_alias}")->fetchField();
      $optimal_limit += $aliases * 250;
    }
  }
  return $optimal_limit;
}

/**
 * Calculate the optimal memory level for sitemap generation.
 *
 * @param $new_limit
 *   An optional PHP memory limit in bytes. If not provided, the value of
 *   _xmlsitemap_get_optimal_memory_limit() will be used.
 */
function _xmlsitemap_set_memory_limit($new_limit = NULL) {
  $current_limit = @ini_get('memory_limit');
  if ($current_limit && $current_limit != -1) {
    if (!is_null($new_limit)) {
      $new_limit = _xmlsitemap_get_optimal_memory_limit();
    }
    if (parse_size($current_limit) < $new_limit) {
      return @ini_set('memory_limit', $new_limit);
    }
  }
}

/**
 * Perform operations after rebuilding the sitemap.
 */
function _xmlsitemap_regenerate_after() {
  // Show a watchdog message that the sitemap was regenerated.
  watchdog('xmlsitemap',
    'Finished XML sitemap generation in @timer ms. Memory usage: @memory-peak.',
    array(
      '@timer' => timer_read('xmlsitemap_regenerate'),
      '@memory-peak' => format_size(memory_get_peak_usage(TRUE)),
    ),
    WATCHDOG_NOTICE
  );

  // Unset the regenerate flag.
  variable_set('xmlsitemap_regenerate_needed', FALSE);

  variable_set('xmlsitemap_generated_last', REQUEST_TIME);
}

/**
 * Fetch the data from {xmlsitemap}, generates the sitemap, then caches it.
 *
 * @param $sitemap
 *   An unserialized data array for an XML sitemap.
 * @param $chunk
 *   An integer representing the integer of the sitemap page chunk.
 * @return
 *   TRUE on success; otherwise FALSE
 *
 * @todo Revise/simplify or remove the function.
 */
function xmlsitemap_generate(array $sitemap, $chunk) {
  if ($chunk != 'index' && !is_numeric($chunk)) {
    // Don't bother translating this string.
    trigger_error('Improper condition hit in xmlsitemap_generate(). Chunk: ' . $chunk);
    return FALSE;
  }

  $file = xmlsitemap_sitemap_get_file($sitemap, $chunk);
  $writer = new XMLSitemapWriter();
  if (!$writer->openURI($file)) {
    trigger_error(t('Could not open file @file for writing.', array('@file' => $file)));
    return FALSE;
  }

  if ($chunk == 'index') {
    xmlsitemap_generate_index($sitemap, $writer);
  }
  else {
    xmlsitemap_generate_chunk($sitemap, $writer, $chunk);
  }

  // End and flush the XML file.
  $writer->endDocument();

  // Track the maximum filesize.
  $filesize = filesize($file);
  if ($filesize > variable_get('xmlsitemap_max_filesize', 0)) {
    variable_set('xmlsitemap_max_filesize', $filesize);
  }

  if (!$writer->getStatus()) {
    trigger_error(t('Unknown error occurred while writing to file @file.', array('@file' => $file)));
    return FALSE;
  }
  //elseif (xmlsitemap_var('gz')) {
  //  $file_gz = $file . '.gz';
  //  file_put_contents($file_gz, gzencode(file_get_contents($file), 9));
  //}

  return $writer->getSitemapElementCount();
}

/**
 * Write the proper XML sitemap header.
 *
 * @param $type
 *   The type of sitemap to generate, either 'sitemapindex' or 'urlset'.
 * @param $sitemap
 *   An unserialized data array for an XML sitemap.
 * @param $writer
 *   The XMLWriter object to write to.
 */
function xmlsitemap_generate_chunk_header($type, array $sitemap, XMLWriter $writer) {
  // Add the stylesheet link.
  if (variable_get('xmlsitemap_xsl', 1)) {
    $xsl_url = url('sitemap.xsl');
    $writer->writePi('xml-stylesheet', 'type="text/xsl" href="' . $xsl_url . '"');
    $writer->writeRaw(PHP_EOL);
  }

  $writer->startElement($type);
  $writer->writeAttribute('xmlns', 'http://www.sitemaps.org/schemas/sitemap/0.9');
  //$writer->writeAttribute('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance');
  //$writer->writeAttribute('xsi:schemaLocation', 'http://www.sitemaps.org/schemas/sitemap/0.9');
  //$schemas = array('sitemapindex' => 'siteindex.xsd', 'urlset' => 'sitemap.xsd');
  //$writer->writeAttribute('http://www.sitemaps.org/schemas/sitemap/0.9/' . $schemas[$type], '');
  $writer->writeRaw(PHP_EOL);
  $writer->flush();
}

/**
 * Generate one page (chunk) of the sitemap.
 *
 * @param $sitemap
 *   An unserialized data array for an XML sitemap.
 * @param $writer
 *   The XMLWriter object to write to.
 * @param $chunk
 *   An integer representing the integer of the sitemap page chunk.
 */
function xmlsitemap_generate_chunk(array $sitemap, XMLSitemapWriter $writer, $chunk) {
  $lastmod_format = variable_get('xmlsitemap_lastmod_format', XMLSITEMAP_LASTMOD_MEDIUM);

  $url_options = $sitemap['uri']['options'];
  $url_options += array(
    'absolute' => TRUE,
    'base_url' => variable_get('xmlsitemap_base_url', $GLOBALS['base_url']),
    'language' => language_default(),
    'alias' => variable_get('xmlsitemap_prefetch_aliases', TRUE),
  );

  $last_url = '';
  $link_count = 0;

  $query = db_select('xmlsitemap', 'x');
  $query->fields('x', array('loc', 'lastmod', 'changefreq', 'changecount', 'priority', 'language', 'access', 'status'));
  $query->condition('access', 1);
  $query->condition('status', 1);
  $query->orderBy('language', 'DESC');
  $query->orderBy('loc');
  $query->addTag('xmlsitemap_generate');
  $query->addMetaData('sitemap', $sitemap);

  $offset = max($chunk - 1, 0) * xmlsitemap_get_chunk_size();
  $limit = xmlsitemap_get_chunk_size();
  $query->range($offset, $limit);
  $links = $query->execute();

  // Add the XML header and XSL if desired.
  xmlsitemap_generate_chunk_header('urlset', $sitemap, $writer);

  while ($link = $links->fetchAssoc()) {
    $link['language'] = $link['language'] != LANGUAGE_NONE ? xmlsitemap_language_load($link['language']) : $url_options['language'];
    if ($url_options['alias']) {
      $link['loc'] = xmlsitemap_get_path_alias($link['loc'], $link['language']->language);
    }
    $link_url = url($link['loc'], array('language' => $link['language']) + $url_options);

    // Skip this link if it was a duplicate of the last one.
    // @todo Figure out a way to do this before generation so we can report
    // back to the user about this.
    if ($link_url == $last_url) {
      continue;
    }
    else {
      $last_url = $link_url;
      // Keep track of the total number of links written.
      $link_count++;
    }

    $element = array();
    $element['loc'] = $link_url;
    if ($link['lastmod']) {
      $element['lastmod'] = gmdate($lastmod_format, $link['lastmod']);
      // If the link has a lastmod value, update the changefreq so that links
      // with a short changefreq but updated two years ago show decay.
      // We use abs() here just incase items were created on this same cron run
      // because lastmod would be greater than REQUEST_TIME.
      $link['changefreq'] = (abs(REQUEST_TIME - $link['lastmod']) + $link['changefreq']) / 2;
    }
    if ($link['changefreq']) {
      $element['changefreq'] = xmlsitemap_get_changefreq($link['changefreq']);
    }
    if (isset($link['priority']) && $link['priority'] != 0.5) {
      // Don't output the priority value for links that have 0.5 priority. This
      // is the default 'assumed' value if priority is not included as per the
      // sitemaps.org specification.
      $element['priority'] = number_format($link['priority'], 1);
    }
    $writer->writeSitemapElement('url', $element, $sitemap);
  }

  return $link_count;
}

/**
 * Generate the index sitemap.
 *
 * @param $sitemap
 *   An unserialized data array for an XML sitemap.
 * @param $writer
 *   The XMLWriter object to write to.
 * @param $status
 */
function xmlsitemap_generate_index(array $sitemap, XMLSitemapWriter $writer) {
  $lastmod_format = variable_get('xmlsitemap_lastmod_format', XMLSITEMAP_LASTMOD_MEDIUM);

  $url_options = $sitemap['uri']['options'];
  $url_options += array(
    'absolute' => TRUE,
    'base_url' => variable_get('xmlsitemap_base_url', $GLOBALS['base_url']),
    'language' => language_default(),
    'alias' => TRUE,
  );

  // Add the XML header and XSL if desired.
  xmlsitemap_generate_chunk_header('sitemapindex', $sitemap, $writer);

  for ($i = 1; $i <= $sitemap['chunks']; $i++) {
    $url_options['query']['page'] = $i;
    $element = array(
      'loc' => url('sitemap.xml', $url_options),
      // @todo Use the actual lastmod value of the chunk file.
      'lastmod' => gmdate($lastmod_format, REQUEST_TIME),
    );
    $writer->writeSitemapElement('sitemap', $element, $sitemap);
  }
}

// BATCH OPERATIONS ------------------------------------------------------------

/**
 * Batch information callback for regenerating the sitemap files.
 *
 * @param $smids
 *   An optional array of XML sitemap IDs. If not provided, it will load all
 *   existing XML sitemaps.
 */
function xmlsitemap_regenerate_batch(array $smids = array()) {
  if (empty($smids)) {
    $smids = db_query("SELECT smid FROM {xmlsitemap_sitemap}")->fetchCol();
  }

  //$t = get_t();
  $batch = array(
    'operations' => array(),
    //'error_message' => $t('An error has occurred.'),
    'finished' => 'xmlsitemap_regenerate_batch_finished',
    'title' => t('Regenerating Sitemap'),
    'file' => drupal_get_path('module', 'xmlsitemap') . '/xmlsitemap.generate.inc',
  );

  // Generate all the sitemap pages for each context.
  $batch['operations'][] = array('_xmlsitemap_regenerate_before', array());
  foreach ($smids as $smid) {
    $batch['operations'][] = array('xmlsitemap_regenerate_batch_generate', array($smid));
    $batch['operations'][] = array('xmlsitemap_regenerate_batch_generate_index', array($smid));
  }
  $batch['operations'][] = array('_xmlsitemap_regenerate_after', array());

  return $batch;
}

/**
 * Batch callback; generate all pages of a sitemap.
 */
function xmlsitemap_regenerate_batch_generate($smid, array &$context) {
  if (!isset($context['sandbox']['sitemap'])) {
    $context['sandbox']['sitemap'] = xmlsitemap_sitemap_load($smid);
    $context['sandbox']['sitemap']['chunks'] = 1;
    $context['sandbox']['sitemap']['links'] = 0;
    $context['sandbox']['max'] = XMLSITEMAP_MAX_SITEMAP_LINKS;

    // Clear the cache directory for this sitemap before generating any files.
    xmlsitemap_check_directory($context['sandbox']['sitemap']);
    xmlsitemap_clear_directory($context['sandbox']['sitemap']);
  }

  $sitemap = &$context['sandbox']['sitemap'];
  $links = xmlsitemap_generate($sitemap, $sitemap['chunks']);
  $context['message'] = t('Now generating %sitemap-url.', array('%sitemap-url' => url('sitemap.xml', $sitemap['uri']['options'] + array('query' => array('page' => $sitemap['chunks'])))));

  if ($links) {
    $sitemap['links'] += $links;
    $sitemap['chunks']++;
  }
  else {
    // Cleanup the 'extra' empty file.
    $file = xmlsitemap_sitemap_get_file($sitemap, $sitemap['chunks']);
    if (file_exists($file) && $sitemap['chunks'] > 1) {
      file_unmanaged_delete($file);
    }
    $sitemap['chunks']--;

    // Save the updated chunks and links values.
    $context['sandbox']['max'] = $sitemap['chunks'];
    $sitemap['updated'] = REQUEST_TIME;
    xmlsitemap_sitemap_save($sitemap);
  }

  if ($sitemap['chunks'] != $context['sandbox']['max']) {
    $context['finished'] = $sitemap['chunks'] / $context['sandbox']['max'];
  }
}

/**
 * Batch callback; generate the index page of a sitemap.
 */
function xmlsitemap_regenerate_batch_generate_index($smid, array &$context) {
  $sitemap = xmlsitemap_sitemap_load($smid);
  if ($sitemap['chunks'] > 1) {
    xmlsitemap_generate($sitemap, 'index');
    $context['message'] = t('Now generating sitemap index %sitemap-url.', array('%sitemap-url' => url('sitemap.xml', $sitemap['uri']['options'])));
  }
}

/**
 * Batch callback; sitemap regeneration finished.
 */
function xmlsitemap_regenerate_batch_finished($success, $results, $operations) {
  if ($success) {
    // Reset the rebuild flag since it was successful.
    variable_set('xmlsitemap_regenerate_needed', FALSE);
    //drupal_set_message(t('The sitemaps were regenerated.'));
  }
  else {
    drupal_set_message(t('The sitemaps was not successfully regenerated.'), 'error');
  }
}

/**
 * Batch information callback for rebuilding the sitemap data.
 */
function xmlsitemap_rebuild_batch(array $entities, $save_custom = FALSE) {
  $batch = array(
    'operations' => array(),
    'finished' => 'xmlsitemap_rebuild_batch_finished',
    'title' => t('Rebuilding Sitemap'),
    'file' => drupal_get_path('module', 'xmlsitemap') . '/xmlsitemap.generate.inc',
  );

  $batch['operations'][] = array('xmlsitemap_rebuild_batch_prerebuild', array());

  // Purge any links first.
  $batch['operations'][] = array('xmlsitemap_rebuild_batch_clear', array($entities, (bool) $save_custom));

  // Fetch all the sitemap links and save them to the {xmlsitemap} table.
  foreach ($entities as $entity) {
    $info = xmlsitemap_get_link_info($entity);
    $batch['operations'][] = array($info['xmlsitemap']['rebuild callback'], array($entity));
  }

  // Add the regeneration batch.
  $regenerate_batch = xmlsitemap_regenerate_batch();
  $batch['operations'] = array_merge($batch['operations'], $regenerate_batch['operations']);

  return $batch;
}

/**
 * Batch callback; perform operations before rebuilding the sitemap data.
 */
function xmlsitemap_rebuild_batch_prerebuild() {
  // Set the rebuild flag in case something fails during the rebuild.
  variable_set('xmlsitemap_rebuild_needed', TRUE);

  timer_start('xmlsitemap_rebuild');
}

/**
 * Batch callback; clear sitemap links for entites.
 */
function xmlsitemap_rebuild_batch_clear(array $entities, $save_custom, &$context) {
  if (!empty($entities)) {
    $query = db_delete('xmlsitemap');
    $query->condition('type', $entities);

    // If we want to save the custom data, make sure to exclude any links
    // that are not using default inclusion or priority.
    if ($save_custom) {
      $query->condition('status_override', 0);
      $query->condition('priority_override', 0);
    }

    $query->execute();
  }

  $context['message'] = t('Purging links.');
}

/**
 * Batch callback; fetch and add the sitemap links for a specific entity.
 */
function xmlsitemap_rebuild_batch_fetch($entity, &$context) {
  if (!isset($context['sandbox']['info'])) {
    $context['sandbox']['info'] = xmlsitemap_get_link_info($entity);
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['last_id'] = 0;
  }
  $info = $context['sandbox']['info'];

  $query = db_select($info['base table'], 'base');
  $query->addField('base', $info['entity keys']['id']);
  $query->condition($info['entity keys']['id'], $context['sandbox']['last_id'], '>');
  $query->addTag('xmlsitemap_link_bundle_access');
  $query->addTag('xmlsitemap_rebuild');
  $query->addMetaData('entity', $entity);
  $query->addMetaData('entity_info', $info);

  if (!isset($context['sandbox']['max'])) {
    $context['sandbox']['max'] = $query->countQuery()->execute()->fetchField();
    if (!$context['sandbox']['max']) {
      // If there are no items to process, skip everything else.
      return;
    }
  }

  // PostgreSQL cannot have the ORDERED BY in the count query.
  $query->orderBy($info['entity keys']['id']);

  $limit = 20; //variable_get('xmlsitemap_batch_limit', 100)
  $query->range(0, $limit);

  $ids = $query->execute()->fetchCol();
  $info['xmlsitemap']['process callback']($ids);
  $context['sandbox']['last_id'] = end($ids);
  $context['sandbox']['progress'] += count($ids);
  $context['message'] = t('Now processing %entity @last_id (@progress of @count).', array('%entity' => $entity, '@last_id' => $context['sandbox']['last_id'], '@progress' => $context['sandbox']['progress'], '@count' => $context['sandbox']['max']));

  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }
}

/**
 * Batch callback; sitemap rebuild finished.
 */
function xmlsitemap_rebuild_batch_finished($success, $results, $operations) {
  if ($success) {
    // Reset the rebuild flag since it was successful.
    variable_set('xmlsitemap_rebuild_needed', FALSE);
    drupal_set_message(t('The sitemap was rebuilt.'));
  }
  else {
    drupal_set_message(t('The sitemap was not successfully rebuilt.'), 'error');
  }
}

function xmlsitemap_get_rebuildable_link_types() {
  $rebuild_types = array();
  $entities = xmlsitemap_get_link_info();

  foreach ($entities as $entity => $info) {
    if (empty($info['xmlsitemap']['rebuild callback'])) {
      // If the entity is missing a rebuild callback, skip.
      continue;
    }
    if (!empty($info['entity keys']['bundle']) && !xmlsitemap_get_link_type_enabled_bundles($entity)) {
      // If the entity has bundles, but no enabled bundles, skip since
      // rebuilding wouldn't get any links.
      continue;
    }
    else {
      $rebuild_types[] = $entity;
    }
  }

  return $rebuild_types;
}
