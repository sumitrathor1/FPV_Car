<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') { http_response_code(204); exit; }

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';

// Security: Enforce strict rec_ folder naming to prevent directory traversal
if (empty($folder) || !preg_match('/^rec_[a-zA-Z0-9_-]+$/', $folder)) {
    http_response_code(400);
    echo json_encode(["error" => "Invalid folder"]);
    exit;
}

$baseDir = __DIR__;
$folderPath = "$baseDir/$folder";

$frames = [];
if (is_dir($folderPath)) {
    $files = glob("$folderPath/*.jpg");
    if ($files) {
        sort($files);
        foreach ($files as $f) {
            $frames[] = basename($f);
        }
    }
}

echo json_encode($frames);
