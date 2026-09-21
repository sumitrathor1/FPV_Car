<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';

if (empty($folder) || !preg_match('/^rec_[a-zA-Z0-9_-]+$/', $folder)) {
    http_response_code(400);
    echo json_encode(["error" => "Invalid folder"]);
    exit;
}

$folderPath = __DIR__ . "/$folder";
if (is_dir($folderPath)) {
    $files = glob($folderPath . '/*');
    foreach ($files as $file) {
        if (is_file($file)) unlink($file);
    }
    rmdir($folderPath);
    echo json_encode(["status" => "deleted"]);
} else {
    http_response_code(404);
    echo json_encode(["error" => "Folder not found"]);
}
