<?php
// $Id$

/**
 * @file
 * Miscellaneous functions for the xmlsitemap module.
 *
 * @ingroup xmlsitemap
 */

/**
 * Fetch a short blurb string about module maintainership and sponsors.
 *
 * This message will be FALSE in 'official' releases.
 */
function _xmlsitemap_get_blurb($check_version = TRUE) {
  static $blurb;

  if (!isset($blurb)) {
    $blurb = FALSE;
    if (!$check_version || (($version = _xmlsitemap_get_version()) && preg_match('/dev|unstable|alpha|beta|HEAD/i', $version))) {
      $sponsors = array(
        l('Symantec', 'http://www.symantec.com/'),
        l('WebWise Solutions', 'http://www.webwiseone.com/'),
        l('Volacci', 'http://www.volacci.com/'),
        l('lanetro', 'http://www.lanetro.com/'),
        l('Coupons Dealuxe', 'http://couponsdealuxe.com/'),
      );
      // Don't extract the following string for translation.
      $blurb = '<div class="description"><p>Thank you for helping test the XML sitemap module rewrite. Please consider helping offset developer free time by <a href="http://davereid.chipin.com/">donating</a> or if your company is interested in sponsoring the rewrite or a specific feature, please <a href="http://davereid.net/contact">contact the developer</a>. Thank you to the following current sponsors: ' . implode(', ', $sponsors) . ', and all the indivduals that have donated. This message will not be seen in the stable versions.</p></div>';
      //http://drupalmodules.com/module/xml-sitemap
    }
  }

  return $blurb;
}

function _xmlsitemap_get_version() {
  static $version;
  if (!isset($version)) {
    $modules = _system_rebuild_module_data();
    $version = $modules['xmlsitemap']->info['version'];
  }
  return $version;
}
