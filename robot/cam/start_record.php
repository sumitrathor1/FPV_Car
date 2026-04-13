<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$folder = "rec_" . date("Ymd_His");
$folderPath = "$baseDir/$folder";

if (!is_dir($folderPath)) {
    mkdir($folderPath, 0755, true);
}

file_put_contents("$baseDir/record.txt", $folder);

echo json_encode(["status" => "recording", "folder" => $folder]);

file_put_contents($recDir . "/index.php", $indexFile);
file_put_contents(__DIR__ . "/record.txt", $folder);

echo $folder;
