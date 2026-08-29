<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(204); exit; }

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';
if (empty($folder) || strpos($folder, '..') !== false) {
    http_response_code(400);
    echo json_encode("Invalid folder");
    exit;
}

$baseDir = __DIR__;
$folderPath = "$baseDir/$folder";

if (is_dir($folderPath)) {
    $files = glob("$folderPath/*");
    foreach ($files as $file) {
        if (is_file($file)) unlink($file);
    }
    rmdir($folderPath);
}

$mp4File = "$baseDir/{$folder}.mp4";
if (file_exists($mp4File)) {
    unlink($mp4File);
}

echo "OK";
