# Sensors

All of the resources for getting data from the cow into BigQuery

# Projects & Structure

## Firmware

This folder contains all of the Embedded and C projects that will be flashed to the devices operating on the farm

### Firmware / PAM

The Firmware that runs on the 2B Tech PAM device

### Firmware / MKR

The Firmware that runs on the Arduino MKR board on the 2B Tech PAM, which provides cellular services

## Cloud

All of the code that has been deployed to GCP

### Cloud / Cloud-Functions

Cloud Functions! These are exposed via an HTTP endpoint that GCP manages for us, and run in response to an HTTP request

#### Cloud / Cloud-Functions / hologram-ingest

When the PAM uploads data, it is first sent to Hologram through their Embedded Socket API. This is a low overhead protocol that minimizes bandwidth from our embedded device. Hologram then sends the data to a webhook that we have defined. In this case, the webhook is this Cloud Function. The Cloud Function simply takes the data sent from Hologram and inserts it into a raw table in BigQuery (`sensors_v1.raw_backpack_v1`).

### Cloud / Cloud-Run

Cloud Run Jobs!

#### Cloud / Cloud-Run / hologram-transform

This Cloud Run job is triggered nightly be an associated Cloud Scheduled Trigger. The job is responisble for taking the data that was inserted into `raw_backpack_v1` during the previous day and inserting it into `backpack_data_v1`. This second table has a schema that represents the data that was collected, rather than the data that was uploaded to Hologram.