# coding: utf-8
from gpmf import parser


def test_gpmf_reader(samples_dir):
    streams = parser.parse_gpmf(samples_dir.join('hero5.mp4'))

    assert streams.keys() == ['ACCL', 'SHUT', 'GPS5', 'ISOG', 'GYRO']

    assert len(streams['ACCL']['values']) == 6870
    assert len(streams['SHUT']['values']) == 816
    assert len(streams['GPS5']['values']) == 618
    assert len(streams['ISOG']['values']) == 816
    assert len(streams['GYRO']['values']) == 13741

    assert len(streams['ACCL']['values'][0]) == 3
    assert len(streams['SHUT']['values'][0]) == 1
    assert len(streams['GPS5']['values'][0]) == 5
    assert len(streams['ISOG']['values'][0]) == 1
    assert len(streams['GYRO']['values'][0]) == 3

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
