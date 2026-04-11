<?php

if (isset($_GET['cmd'])) {
    $cmd = $_GET['cmd'];

    $allowed = ['F','B','L','R','S'];

    if (in_array($cmd, $allowed)) {
        file_put_contents("state.txt", $cmd);
        echo "OK";
    } else {
        echo "Invalid";
    }

} else {
    echo "No command";
}