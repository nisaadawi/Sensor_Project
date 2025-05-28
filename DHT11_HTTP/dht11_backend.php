<?php
include_once("dbconfig.php");

// Get parameters from GET request
$device_id = $_GET['device_id'];
$temp = $_GET['temperature'];
$hum = $_GET['humidity'];
//$relay_status = $_GET['relay_status'] ?? 0;

// Prepare SQL statement with parameters
$sqlInsert = "INSERT INTO `tbl_dht11`(`device_id`, `temperature`, `humidity`) 
                         VALUES ('$device_id','$temp','$hum')";
//$stmt->bind_param("sdii", $device_id, $temp, $hum, $relay_status);

if ($conn->query($sqlInsert) === TRUE) {
    echo "success";
} else {
    echo "failed";
}

?>