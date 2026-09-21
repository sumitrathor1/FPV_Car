<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';

if (empty($folder) || !preg_match('/^rec_[a-zA-Z0-9_-]+$/', $folder)) {
    echo json_encode([]);
    exit;
}

$folderPath = __DIR__ . "/$folder";
if (!is_dir($folderPath)) {
    echo json_encode([]);
    exit;
}

$frames = [];
$items = scandir($folderPath);
foreach ($items as $item) {
    if (pathinfo($item, PATHINFO_EXTENSION) === 'jpg') {
        $frames[] = $item;
    }
}

sort($frames, SORT_NATURAL);
echo json_encode($frames);
