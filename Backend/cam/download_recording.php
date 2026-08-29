<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

$method = $_SERVER['REQUEST_METHOD'] ?? '';
if ($method === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';
$type = isset($_GET['type']) ? basename($_GET['type']) : 'zip';

if (empty($folder) || strpos($folder, '..') !== false) {
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

if ($type === 'mp4') {
    $mp4File = "$baseDir/{$folder}.mp4";
    if (!file_exists($mp4File)) {
        http_response_code(404);
        echo json_encode(["error" => "MP4 file not found, convert first"]);
        exit;
    }

    header("Content-Type: video/mp4");
    header("Content-Disposition: attachment; filename=\"{$folder}.mp4\"");
    header("Content-Length: " . filesize($mp4File));
    readfile($mp4File);
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

unlink($zipFile);
exit;
