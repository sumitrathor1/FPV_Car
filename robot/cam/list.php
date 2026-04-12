<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$dirs = glob(__DIR__ . '/rec_*', GLOB_ONLYDIR);
$names = array_map('basename', $dirs ?: []);
echo json_encode(array_values($names));
