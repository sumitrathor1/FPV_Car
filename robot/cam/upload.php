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

                $indexFile = <<<'PHP'
<?php
$images = glob(__DIR__ . '/*.jpg');
sort($images);
?><!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Recording Viewer</title>
    <style>
        body { margin: 0; padding: 16px; background: #081620; color: #fff; font-family: Arial, sans-serif; }
        h1 { margin: 0 0 16px; font-size: 20px; }
        .grid { display: grid; gap: 12px; max-width: 980px; margin: 0 auto; }
        .item { border: 1px solid rgba(255,255,255,0.12); border-radius: 10px; overflow: hidden; background: rgba(255,255,255,0.04); }
        img { width: 100%; display: block; height: auto; }
        .empty { color: #a8bfd1; }
    </style>
</head>
<body>
    <div class="grid">
        <h1><?= htmlspecialchars(basename(__DIR__), ENT_QUOTES, 'UTF-8') ?></h1>
        <?php if (!$images): ?>
            <div class="empty">No images yet.</div>
        <?php endif; ?>
        <?php foreach ($images as $image): ?>
            <div class="item"><img src="<?= htmlspecialchars(basename($image), ENT_QUOTES, 'UTF-8') ?>" alt=""></div>
        <?php endforeach; ?>
    </div>
</body>
</html>
PHP;

                file_put_contents($recDir . "/index.php", $indexFile);

        file_put_contents("$recDir/$time.jpg", $rawData);
    }
}

echo "OK";
