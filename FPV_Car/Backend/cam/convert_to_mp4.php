<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");
header("Content-Type: application/json");

$method = $_SERVER['REQUEST_METHOD'] ?? '';
if ($method === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$folder = isset($_GET['folder']) ? basename($_GET['folder']) : '';

// Security: Enforce strict rec_ folder naming to prevent directory traversal
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

$frames = glob($folderPath . '/*.jpg');
if (empty($frames)) {
    http_response_code(400);
    echo json_encode(["error" => "No frames in folder"]);
    exit;
}

$outputFile = "$baseDir/{$folder}.mp4";

if (file_exists($outputFile)) {
    echo json_encode([
        "status" => "success",
        "file" => "{$folder}.mp4",
        "size" => filesize($outputFile),
        "cached" => true
    ]);
    exit;
}

exec("cd " . escapeshellarg($folderPath) . " && ffmpeg -y -framerate 10 -pattern_type glob -i '*.jpg' -c:v libx264 -pix_fmt yuv420p -crf 28 " . escapeshellarg($outputFile) . " 2>&1", $output, $returnCode);

if ($returnCode === 0 && file_exists($outputFile)) {
    echo json_encode([
        "status" => "success",
        "file" => "{$folder}.mp4",
        "size" => filesize($outputFile)
    ]);
} else {
    echo json_encode([
        "status" => "error",
        "message" => "FFmpeg conversion failed",
        "output" => implode("\n", $output)
    ]);
}
