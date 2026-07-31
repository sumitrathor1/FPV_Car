<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';
$folderPath = "$baseDir/$folder";

$frames = [];
if ($folder !== '' && is_dir($folderPath)) {
    foreach (scandir($folderPath) as $item) {
        if (pathinfo($item, PATHINFO_EXTENSION) === 'jpg') {
            $frames[] = $item;
        }
    }
    sort($frames);
}

echo json_encode($frames);
