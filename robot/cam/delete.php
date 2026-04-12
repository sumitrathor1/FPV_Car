<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';

if ($folder === '' || !is_dir($folder)) {
    http_response_code(400);
    echo "INVALID_FOLDER";
    exit;
}

function deleteFolder($dir) {
    foreach (glob($dir . '/*') as $file) {
        if (is_dir($file)) {
            deleteFolder($file);
        } else {
            unlink($file);
        }
    }
    rmdir($dir);
}

deleteFolder($folder);
echo "DELETED";
