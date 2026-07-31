<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(204); exit; }

$baseDir = __DIR__;
$recordFile = "$baseDir/record.txt";

$folder = "rec_" . date("Ymd_His");
$folderPath = "$baseDir/$folder";

if (!is_dir($folderPath)) {
    mkdir($folderPath, 0755, true);
}

file_put_contents($recordFile, $folder);

echo json_encode(["status" => "recording", "folder" => $folder]);
