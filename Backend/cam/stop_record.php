<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(204); exit; }

$baseDir = __DIR__;
$recordFile = "$baseDir/record.txt";

if (file_exists($recordFile)) {
    unlink($recordFile);
}

echo json_encode(["status" => "stopped"]);
