<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';
if ($folder === '') {
    http_response_code(400);
    echo json_encode([]);
    exit;
}

$folderPath = __DIR__ . '/' . $folder;
if (!is_dir($folderPath)) {
    http_response_code(404);
    echo json_encode([]);
    exit;
}

$frames = glob($folderPath . '/*.jpg') ?: [];
sort($frames, SORT_NATURAL);
$names = array_map('basename', $frames);
echo json_encode($names);
