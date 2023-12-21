# Build the project

```cmd
$ gcloud builds submit --pack image=gcr.io/sensor-data-382014/hologram-transform-job
```

# Deploy the project locally
```cmd
$ docker pull gcr.io/sensor-data-382014/hologram-transform-job:latest
$ docker run ^
    --rm ^
    -e RUMISSION_DATE_PART=2023-04-03 ^
    -e K_SERVICE=dev ^
    -e K_CONFIGURATION=dev ^
    -e K_REVISION=dev-00001 ^
    -e GCLOUD_PROJECT=sensor-data-382014 ^
    -e GOOGLE_APPLICATION_CREDENTIALS=/tmp/keys/asdf.json ^
    -v %APPDATA%\gcloud\application_default_credentials.json:/tmp/keys/asdf.json ^
    gcr.io/sensor-data-382014/hologram-transform-job:latest
```

# Deploy to GCP
```cmd
$ gcloud beta run jobs create hologram-transform ^
    --image gcr.io/sensor-data-382014/hologram-transform-job ^
    --tasks 1 ^
    --max-retries 5 ^
    --region us-central1
```