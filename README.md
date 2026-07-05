# rocket_thrust — KNSB 추력 측정

10kg 로드셀 + HX711로 고체연료 그레인 연소 추력 프로파일 측정. HC-05 블루투스로 원격 START/STOP/TARE.

## 보드
- Arduino Uno (또는 Nano Classic)
- HC-05 블루투스 모듈 (9600bps 기본)
- 10kg 로드셀 + HX711 ADC

## 배선

### HX711 → Uno
| HX711 | Uno |
|-------|-----|
| DOUT  | D2  |
| SCK   | D3  |
| VCC   | 5V  |
| GND   | GND |

### HC-05 → Uno
| HC-05 | Uno |
|-------|-----|
| TX    | D10 (RX) |
| RX    | D11 (TX) — **5V→3.3V 분압저항 권장** (1kΩ + 2kΩ) |
| VCC   | 5V  |
| GND   | GND |

### 로드셀 → HX711
4선 (E+/E-/A+/A-) — HX711 보드 라벨대로 연결.

## 사용 순서
1. 로드셀 장착 (매달거나 고정하고 역방향 추력 받도록 배치)
2. 보드 전원 인가 → 5~10초 대기 (영점 안정화)
3. `TARE` 명령
4. `START` → 20초 측정 (100Hz, 2000샘플)
5. 출력 CSV: `time_ms,thrust_kg`
6. 20초 후 자동 종료, LED 점등

## 명령어
| 명령 | 동작 |
|------|------|
| `TARE` | 영점 |
| `START` | 20초 측정 시작 |
| `STOP` | 강제 중단 |
| `CAL <val>` | scale 보정값 (예: `CAL 2280.0`) |
| `STATUS` | 현재 상태/영점 |

## 보정 (CAL)
`DEFAULT_SCALE = 2280.0`은 placeholder. 정확한 값은 알려진 무게(예: 1kg 추)로 보정:

```cpp
scale.set_scale();
scale.tare();
// 1kg 추가 후
long raw = scale.get_value(10);  // 10회 평균
// raw / 1000 = scale (count/kg)
// CAL <그값>
```

## 시리얼 모니터
USB-COM(115200) 또는 HC-05(9600) 둘 다 동일 출력.

## 출력 후처정 (Python)
```python
import pandas as pd
df = pd.read_csv('thrust.csv', names=['t_ms','thrust_kg'])
df['thrust_N'] = df['thrust_kg'] * 9.80665
total_impulse = (df['thrust_N'].sum() * 0.01)  # 10ms 간격 → N·s
max_thrust = df['thrust_N'].max()
```