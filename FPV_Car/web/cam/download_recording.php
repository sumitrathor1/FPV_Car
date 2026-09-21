<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

$method = $_SERVER['REQUEST_METHOD'] ?? '';
if ($method === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';

if (empty($folder) || !preg_match('/^rec_[a-zA-Z0-9_-]+$/', $folder)) {
    http_response_code(400);
    echo json_encode(["error" => "Invalid folder"]);
    exit;
}

$baseDir = __DIR__;
$folderPath = "$baseDir/$folder";

if (!is_dir($folderPath)) {
    http_response_code(404);
    echo json_encode(["error" => "Folder not found"]);
    exit;
}

if (!class_exists('ZipArchive')) {
    http_response_code(500);
    echo json_encode(["error" => "ZipArchive not enabled on server"]);
    exit;
}

$zipFile = tempnam(sys_get_temp_dir(), "{$folder}_") . ".zip";
$zip = new ZipArchive();
if ($zip->open($zipFile, ZipArchive::CREATE) !== true) {
    http_response_code(500);
    echo json_encode(["error" => "Failed to create ZIP"]);
    exit;
}

$files = glob($folderPath . '/*.jpg');
sort($files, SORT_NATURAL);

foreach ($files as $file) {
    $filename = basename($file);
    $zip->addFile($file, $filename);
}

$zip->close();

header("Content-Type: application/zip");
header("Content-Disposition: attachment; filename=\"{$folder}.zip\"");
header("Content-Length: " . filesize($zipFile));
readfile($zipFile);

@unlink($zipFile);
exit;
