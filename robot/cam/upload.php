<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$baseDir = __DIR__;
$rawData = file_get_contents("php://input");

file_put_contents("$baseDir/latest.jpg", $rawData);

$bufferDir = "$baseDir/buffer";
if (!is_dir($bufferDir)) {
    mkdir($bufferDir);
}

$time = time();
file_put_contents("$bufferDir/$time.jpg", $rawData);

$files = glob("$bufferDir/*.jpg");
if (count($files) > 100) {
    sort($files);
    unlink($files[0]);
}

$recordFile = "$baseDir/record.txt";
if (file_exists($recordFile)) {
    $folder = trim(file_get_contents($recordFile));

    if ($folder !== "") {
        $recDir = "$baseDir/$folder";
        if (!is_dir($recDir)) {
            mkdir($recDir);
        }

        file_put_contents("$recDir/$time.jpg", $rawData);
    }
}

echo "OK";
