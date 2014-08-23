<?php

/**
 * @file
 * Set up session with DataCash.
 *
 * @param array $arguments
 *   key 'order_id' -- unique id of the transaction.
 *   key 'order_currency_code' -- code of the currency (USD, GBP etc.).
 *   key 'order_amount' -- amount of transaction.
 *   key 'return_url' -- URL of the return callback
 *    (where Datacash should redirect user after payment).
 *   key 'datacash_client' -- login of the client in Datacash.
 *   key 'datacash_password' -- password of the client in Datacash.
 *   key 'datacash_server_prefix' -- prefix of the Datacash URL
 *     to communicate with.
 *   key 'merchant_url' -- URL of our shop.
 */
function commerce_datacash_setup_session($arguments) {
  // Extract arguments.
  extract($arguments);

  commerce_datacash_require_library();

  $host = 'https://' . $datacash_server_prefix . 'datacash.com/Transaction';
  $configuration_string = commerce_datacash_configuration_string($host);

  // This is really bad practice to use global variable here, but because
  // of DataCash_Logger uses global variable we should follow.
  global $config;
  $config = new DataCash_Document();
  $config->readDocumentFromString($configuration_string);

  $send_doc = new DataCash_Document('Request');

  $send_doc->set('Request.Authentication.client', $datacash_client);
  $send_doc->set('Request.Authentication.password', $datacash_password);
  $send_doc->set('Request.Transaction.TxnDetails.amount', $order_amount, array('currency' => $order_currency_code));
  $send_doc->set('Request.Transaction.TxnDetails.merchantreference', $datacash_merchant_reference_prefix . $order_id);
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.verify', 'yes');
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.merchant_url', $merchant_url);
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.purchase_datetime', date('Ymd G:i:s'));
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.purchase_desc', 'AAT purchase');
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.Browser.device_category', '0');
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.Browser.accept_headers', '<![CDATA[text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8]]>');
  $send_doc->set('Request.Transaction.TxnDetails.ThreeDSecure.Browser.user_agent', '<![CDATA[' . $_SERVER['HTTP_USER_AGENT'] . ']]>');
  $send_doc->set('Request.Transaction.CardTxn.method', 'auth');
  $send_doc->set('Request.Transaction.HpsTxn.method', 'setup_full');
  $send_doc->set('Request.Transaction.HpsTxn.page_set_id', $datacash_template_id);
  $send_doc->set('Request.Transaction.HpsTxn.return_url', '<![CDATA[' . $return_url . ']]>');

  $agent = new DataCash_SSLAgent($config);
  $success = $agent->send($send_doc);

  $response_doc = $agent->getResponseDocument();
  if ($response_doc->get('Response.status') != 1) {
    drupal_set_message(t('Cannot start DataCash session. Error: %e', array('%e' => $response_doc->get('Response.reason'))), 'error');
    return FALSE;
  }
  else {
    return array(
      'session_id' => $response_doc->get('Response.HpsTxn.session_id'),
      'datacash_reference' => $response_doc->get('Response.datacash_reference'),
    );
  }
}

/**
 * Request status of the transaction from DataCash.
 *
 * Used to check whether payment has been successful or not.
 *
 * @param array $arguments
 *   key 'datacash_reference' -- reference of the Datacash about transaction
 *   key 'datacash_server_prefix' -- prefix of the Datacash server
 *     (testing or production).
 */
function commerce_datacash_payment_status($arguments) {
  // Extract arguments.
  extract($arguments);

  commerce_datacash_require_library();

  $host = 'https://' . $datacash_server_prefix . 'datacash.com/Transaction';
  $configuration_string = commerce_datacash_configuration_string($host);

  // This is really bad practice to use global variable here, but because
  // of DataCash_Logger uses global variable we should follow.
  global $config;
  $config = new DataCash_Document();
  $config->readDocumentFromString($configuration_string);

  $send_doc = new DataCash_Document('Request');

  $send_doc->set('Request.Authentication.client', variable_get('datacash_client', $datacash_client));
  $send_doc->set('Request.Authentication.password', variable_get('datacash_password', $datacash_password));
  $send_doc->set('Request.Transaction.HistoricTxn.method', 'query');
  $send_doc->set('Request.Transaction.HistoricTxn.reference', $datacash_reference);

  $agent = new DataCash_SSLAgent($config);
  $success = $agent->send($send_doc);

  $response_doc = $agent->getResponseDocument();
  if (empty($response_doc) || $response_doc->get('Response.status') != 1) {
    $errror = !empty($response_doc) ? $response_doc->get('Response.reason') : t('Response object is empty.');
    drupal_set_message(t('Cannot start DataCash session. Error: %e', array('%e' => $errror)), 'error');
    return FALSE;
  }
  else {
    return array(
      'status' => $response_doc->get('Response.status'),
      'datacash_reference' => $response_doc->get('Response.datacash_reference'),
      'merchantreference' => $response_doc->get('Response.merchantreference'),
    );
  }
}

/**
 * Prepare configuration string for datacash library.
 *
 * No need for logging.
 */
function commerce_datacash_configuration_string($host) {
  return '
    <Configuration>
      <host>' . $host . '</host>
      <timeout>5</timeout>
      <logging>0</logging>
      <logfile>/tmp/datacash-log-file</logfile>
    </Configuration>
  ';
}

/**
 * Require Datacash library.
 */
function commerce_datacash_require_library() {
  require_once drupal_get_path('module', 'commerce_datacash') . '/DataCash-PHP-XMLAPI/lib/DataCash.php';
}
