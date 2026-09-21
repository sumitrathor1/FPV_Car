<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$folders = [];

$items = scandir($baseDir);
foreach ($items as $item) {
    if ($item === '.' || $item === '..') continue;
    if (is_dir("$baseDir/$item") && strpos($item, 'rec_') === 0) {
        $folders[] = $item;
    }
}

rsort($folders);
echo json_encode($folders);
