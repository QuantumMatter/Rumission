import functions_framework
from google.cloud import bigquery

from flask import Request, Response

import base64

client = bigquery.Client()

@functions_framework.http
def ingest_hologram(request: Request):

    print(request.headers)
    print(request.data)
    print(request.origin)
    print(request.user_agent)

    if request.headers.get('X-Rumission-Key', "") != "$RUMISSION_KEY":
        print('Invalid X-Rumission-Key!')
        return Response(status=403)
    
    if request.user_agent.string != "HologramCloud (https://hologram.io)":
        print('Invalid User Agent!')
        return Response(status=403)

    hologram: dict = request.get_json()

    new_row = {
        'device_id': hologram.get('device_id'),
        'device_name': hologram.get('device_name'),
        'hologram_timestamp': hologram.get('received'),
        'tags': hologram.get('tags', []),
        'error_code': hologram.get('error_code'),
        'data': hologram.get('data'),
    }
    new_row['decoded'] = base64.b64decode(new_row['data']).decode()

    print(new_row)

    return {
        'result': client.insert_rows_json(
            table='sensor-data-382014.sensors_v1.raw_backpack_v1',
            # json_rows=[{ 'message': json.dumps(new_row) }]
            json_rows=[ new_row ]
        )
    }
