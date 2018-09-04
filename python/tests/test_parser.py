# coding: utf-8
from gpmf import parser


def test_gpmf_reader(samples_dir):
    streams = parser.parse_gpmf(samples_dir.join('hero5.mp4'))

    assert streams.keys() == ['ACCL', 'SHUT', 'GPS5', 'ISOG', 'GYRO']

    assert len(streams['ACCL']['values']) == 164880
    assert len(streams['SHUT']['values']) == 6528
    assert len(streams['GPS5']['values']) == 24720
    assert len(streams['ISOG']['values']) == 6528
    assert len(streams['GYRO']['values']) == 329784

    assert streams['ACCL']['units'] == [u'm/s²']
    assert streams['SHUT']['units'] == ['s']
    assert streams['GPS5']['units'] == ['deg', 'deg', 'm', 'm/s', 'm/s']
    assert streams['ISOG']['units'] == []
    assert streams['GYRO']['units'] == ['rad/s']

    assert streams['ACCL']['name'] == \
        'Accelerometer (up/down, right/left, forward/back)'
    assert streams['SHUT']['name'] == 'Exposure time (shutter speed)'
    assert streams['GPS5']['name'] == \
        'GPS (Lat., Long., Alt., 2D speed, 3D speed)'
    assert streams['ISOG']['name'] == 'Sensor gain (ISO x100)'
    assert streams['GYRO']['name'] == 'Gyroscope (z,x,y)'
