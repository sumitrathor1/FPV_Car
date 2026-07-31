<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$folderName = "rec_" . date("Ymd_His");
$recordFile = "$baseDir/record.txt";

file_put_contents($recordFile, $folderName);
echo json_encode(["status" => "success", "folder" => $folderName]);
