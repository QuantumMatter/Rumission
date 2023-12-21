from google.cloud import bigquery
from google.cloud.bigquery.table import RowIterator, Row

import pandas

import os
import math
import json
from datetime import datetime, timezone, timedelta
from collections import OrderedDict
from uuid import uuid4
import tempfile

PROJECT_ID = 'sensor-data-382014'
DATASET_ID = f'{ PROJECT_ID }.sensors_v1'
RAW_TABLE_ID = f'{ DATASET_ID }.raw_backpack_v1'
DATA_TABLE_ID = f'{ DATASET_ID }.backpack_data_v1'

JOB_NAME = os.getenv('CLOUD_RUN_JOB', 'hologram-transform')
EXECUTION_NAME = os.getenv('CLOUD_RUN_EXECUTION', 'hologram-transform-abc')

TASK_INDEX = os.getenv('CLOUD_RUN_TASK_INDEX', 0)
TASK_COUNT = os.getenv('CLOUD_RUN_TASK_COUNT', 1)
TASK_ATTEMPT = os.getenv('CLOUD_RUN_TASK_ATTEMPT', 0)

today = datetime.now(tz=timezone.utc)
yesterday = today - timedelta(days=1)
default_date_part = yesterday.strftime('%Y-%m-%d')
RAW_DATE_PARITION = os.getenv('RUMISSION_DATE_PART', default_date_part)

def main():
    client = bigquery.Client()

    # Drop records from ${DATA_TABLE_ID} and ${SAMPLES_TABLE_ID} that
    # may have already been inserted from ${RAW_TABLE_ID}
    # This will allow us to re-run the job if needed and prevent
    # duplicate data from being introduced in this way

    raw_table = client.get_table(RAW_TABLE_ID)
    data_table = client.get_table(DATA_TABLE_ID)

    delete_query = f'DELETE FROM `{ DATA_TABLE_ID }` WHERE ingest_date = DATE("{ RAW_DATE_PARITION }")'
    print(delete_query)
    delete_results = client.query(delete_query).result()

    to_process_query = f"SELECT * FROM `{ raw_table }` WHERE _PARTITIONDATE = DATE('{ RAW_DATE_PARITION }')"
    to_process_rows: RowIterator = client.query(to_process_query).result()

    temp_fd, temp_filepath = tempfile.mkstemp()
    try:

        with os.fdopen(temp_fd, 'w') as csv_file:

            csv_file.write('id,sample_id,name,value_str,value,value_type,units,ssid,timestamp,sid,ingest_date,ingest_id\n')

            for row in to_process_rows:
                row: Row = row
                data = row.get('decoded')
                if data is None: continue
                
                try:
                    data = json.loads(data)
                    body = data.get('body', '[]')
                    body = json.loads(body)

                    for idx, sample in enumerate(body):

                        new_row = OrderedDict()

                        new_row['id'] = str(uuid4())
                        new_row['sample_id'] = 'None'
                        new_row['name'] = sample.get('name', 'X-ERROR')
                        new_row['value_str'] = sample.get('value', 'X-ERROR')
                        new_row['value'] = "NaN"
                        new_row['value_type'] = sample.get('valueType', 'X-ERROR')
                        new_row['units'] = sample.get('units', 'X-ERROR')
                        new_row['ssid'] = sample.get('ssid')
                        new_row['timestamp'] = datetime.fromtimestamp(data.get('datetime', 0)).isoformat()
                        new_row['sid'] = data.get('sid', 'X-ERROR')
                        new_row['ingest_date'] = RAW_DATE_PARITION
                        new_row['ingest_id'] = row.get('id', 'X-ERROR')

                        if sample.get('valueType') == 0:
                            new_row['value'] = float(sample.get('value'))

                        line = ','.join(map(lambda item: str(item), new_row.values()))
                        csv_file.write(line + '\n')

                except Exception as exc:
                    print(exc)
        
        job_config = bigquery.LoadJobConfig(
            source_format=bigquery.SourceFormat.CSV,
            schema=data_table.schema,
            skip_leading_rows=1,
            autodetect=True
        )
        
        with open(temp_filepath, 'rb') as source_file:
            print(f'Attempting to upload a { os.stat(temp_filepath).st_size } byte file')
            job = client.load_table_from_file(source_file, data_table, job_config=job_config)
        
        result = job.result()
        print(result)

    finally:
        os.remove(temp_filepath)


if __name__ == '__main__':
    print('Running a cloud run job!')
    main()