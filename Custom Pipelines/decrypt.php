<?php

/*
 * Software Name : Live Objects Helper / Custom Pipelines
 * Version: 1.0
 * SPDX-FileCopyrightText: Copyright (c) 2020 Orange Business Services
 * SPDX-License-Identifier: BSD-3-Clause
 * This software is distributed under the BSD-3-Clause,
 * the text of which is available at https://opensource.org/licenses/BSD-3-Clause
 * or see the "LICENCE" file for more details.
 * Software description: Resources to help using Orange Live Objects Custom Pipelines
 */

$encryption_key = "MY_encryption_key-14829101847124";    // 256 bits
$iv = "EATNRvodplet-129";   // 128 bits
// openssl_random_pseudo_bytes($iv_size, $strong);
// $strong will be true if the key is crypto safe

function pkcs7_pad($data, $size)
{
    $length = $size - strlen($data) % $size;
    return $data . str_repeat(chr($length), $length);
}
function pkcs7_unpad($data)
{
    return substr($data, 0, -ord($data[strlen($data) - 1]));
}

function encrypt($data) {
    global $encryption_key, $iv;
    return openssl_encrypt(
        pkcs7_pad($data, 16), // padded data
        'AES-256-CBC',        // cipher and mode
        $encryption_key,      // secret key
        0,                    // options (not used)
        $iv                   // initialisation vector
    );
}
function decrypt($data) {
    global $encryption_key, $iv;
    return pkcs7_unpad(openssl_decrypt(
        $data,
        'AES-256-CBC',
        $encryption_key,
        0,
        $iv
    ));
}

if ($_REQUEST['encrypt'])
    print encrypt($_REQUEST['encrypt']);
else {
    $data = json_decode(file_get_contents('php://input'), true);
    //print_r($data);
    $dec_data = decrypt($data["value"]["payload"]);
    $new_data = [];
    $new_data["value"] = [];
    $new_data["value"]["deciphered"] = json_decode($dec_data);
    $new_data["location"] = $data["location"];
    $new_data["tags"] = $data["tags"];
    header('Content-type: application/json');
    print(json_encode($new_data));
}
?>