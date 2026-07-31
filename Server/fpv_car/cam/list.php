<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$folders = [];

foreach (scandir($baseDir) as $item) {
    if ($item !== "." && $item !== ".." && is_dir("$baseDir/$item") && strpos($item, "rec_") === 0) {
        $folders[] = $item;
    }
}

sort($folders);
echo json_encode($folders);
