<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$data = file_get_contents("php://input");

file_put_contents("$baseDir/latest.jpg", $data);
echo "OK";
