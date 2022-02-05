<?php
require 'vendor/autoload.php';

define('MQTT_HOST',	 '192.168.1.127');
define('MQTT_PORT',  1883);
define('MQTT_USER',	 'raven');
define('MQTT_PASS',	 'bynthytn');
define('MQTT_CL_ID', 'weather-station-8112');
define('DB_USER',    'raven');
define('DB_PASS',    'bynthytn');
define('DB_NAME',    'dacha');

use PhpMqtt\Client\ConnectionSettings;
use PhpMqtt\Client\Exceptions\MqttClientException;
use PhpMqtt\Client\MqttClient;

DB::$user       = DB_USER;
DB::$password   = DB_PASS;
DB::$dbName     = DB_NAME;

try
{
    $client = new MqttClient(MQTT_HOST, MQTT_PORT, MQTT_CL_ID, MqttClient::MQTT_3_1);
    $connectionSettings = (new ConnectionSettings)->setUsername(MQTT_USER)->setPassword(MQTT_PASS);
    $client->connect($connectionSettings, true);
    $client->subscribe('weather', function (string $topic, string $message, bool $retained) use ($client)
    {
        $s = date('r'). "\n";
        $s .= $message. "\n";
        file_put_contents(__DIR__. '/weather-station.log', $s, FILE_APPEND);
        $json = json_decode($message, JSON_OBJECT_AS_ARRAY);
        if (is_array($json))
        {
            $result = DB::insert('weather',
            [
                'ts'    =>  time(),
                'rain'  => $json['r'],
                'light' => $json['l'],
                'temp'  => $json['t'],
                'press' => $json['p'],
                'alt'   => $json['a'],
                'humi'  => $json['h']
            ]);
        }
    }, MqttClient::QOS_AT_MOST_ONCE);
    $client->loop(true);
    $client->disconnect();
}
catch (MqttClientException $e)
{
    die('Connecting with username and password or publishing with QoS 0 failed. An exception occurred.');
}
?>
