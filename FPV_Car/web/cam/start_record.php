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
$folderPath = "$baseDir/$folderName";

if (!is_dir($folderPath)) {
    @mkdir($folderPath, 0755, true);
}

file_put_contents("$baseDir/record.txt", $folderName, LOCK_EX);
echo json_encode(["status" => "recording", "folder" => $folderName]);
