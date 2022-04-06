/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tests.h"
#include <setup.h>
#include <stdio.h>
#include <time.h>

#include "bat_charger.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

//#define DEBUG 0

// GPS Model Backtest of TinyEKF Lib
// See for implemented test example:
// https://github.com/simondlevy/TinyEKF/tree/master/extras/c

/* states */
#define NUMBER_OF_STATES_GPS 8

/* observables */
#define NUMBER_OF_OBSERVABLES_GPS 4

typedef struct
{

    int n; /* number of state values */
    int m; /* number of observables */

    float x[NUMBER_OF_STATES_GPS]; /* state vector */

    float P[NUMBER_OF_STATES_GPS][NUMBER_OF_STATES_GPS]; /* prediction error covariance */
    float Q[NUMBER_OF_STATES_GPS][NUMBER_OF_STATES_GPS]; /* process noise covariance */
    float R[NUMBER_OF_OBSERVABLES_GPS]
           [NUMBER_OF_OBSERVABLES_GPS]; /* measurement error covariance */

    float G[NUMBER_OF_STATES_GPS][NUMBER_OF_OBSERVABLES_GPS]; /* Kalman gain; a.k.a. K */

    float F[NUMBER_OF_STATES_GPS][NUMBER_OF_STATES_GPS];      /* Jacobian of process model */
    float H[NUMBER_OF_OBSERVABLES_GPS][NUMBER_OF_STATES_GPS]; /* Jacobian of measurement model */

    float Ht[NUMBER_OF_STATES_GPS]
            [NUMBER_OF_OBSERVABLES_GPS];                  /* transpose of measurement Jacobian */
    float Ft[NUMBER_OF_STATES_GPS][NUMBER_OF_STATES_GPS]; /* transpose of process Jacobian */
    float Pp[NUMBER_OF_STATES_GPS][NUMBER_OF_STATES_GPS]; /* P, post-prediction, pre-update */

    float fx[NUMBER_OF_STATES_GPS];      /* output of user defined f() state-transition function */
    float hx[NUMBER_OF_OBSERVABLES_GPS]; /* output of user defined h() measurement function */

    /* temporary storage */
    float tmp0[NUMBER_OF_STATES_GPS][NUMBER_OF_STATES_GPS];
    float tmp1[NUMBER_OF_STATES_GPS][NUMBER_OF_OBSERVABLES_GPS];
    float tmp2[NUMBER_OF_OBSERVABLES_GPS][NUMBER_OF_STATES_GPS];
    float tmp3[NUMBER_OF_OBSERVABLES_GPS][NUMBER_OF_OBSERVABLES_GPS];
    float tmp4[NUMBER_OF_OBSERVABLES_GPS][NUMBER_OF_OBSERVABLES_GPS];
    float tmp5[NUMBER_OF_OBSERVABLES_GPS];

} ekf_gps_t;

// positioning interval
static const float T = 1;

static void blk_fill(ekf_gps_t *ekf_gps, const float *a, int off)
{
    off *= 2;

    ekf_gps->Q[off][off] = a[0];
    ekf_gps->Q[off][off + 1] = a[1];
    ekf_gps->Q[off + 1][off] = a[2];
    ekf_gps->Q[off + 1][off + 1] = a[3];
}

static void init_gps(ekf_gps_t *ekf_gps)
{
    // Set Q, see [1]
    const float Sf = 36;
    const float Sg = 0.01;
    const float sigma = 5; // state transition variance
    const float Qb[4] = { Sf * T + Sg * T * T * T / 3, Sg * T * T / 2, Sg * T * T / 2, Sg * T };
    const float Qxyz[4] = { sigma * sigma * T * T * T / 3, sigma * sigma * T * T / 2,
                            sigma * sigma * T * T / 2, sigma * sigma * T };

    blk_fill(ekf_gps, Qxyz, 0);
    blk_fill(ekf_gps, Qxyz, 1);
    blk_fill(ekf_gps, Qxyz, 2);
    blk_fill(ekf_gps, Qb, 3);

    // initial covariances of state noise, measurement noise
    float P0 = 10;
    float R0 = 36;

    int i;

    for (i = 0; i < NUMBER_OF_STATES_GPS; ++i) {
        ekf_gps->P[i][i] = P0;
    }

    for (i = 0; i < NUMBER_OF_OBSERVABLES_GPS; ++i) {
        ekf_gps->R[i][i] = R0;
    }

    // position
    ekf_gps->x[0] = -2.168816181271560e+006;
    ekf_gps->x[2] = 4.386648549091666e+006;
    ekf_gps->x[4] = 4.077161596428751e+006;

    // velocity
    ekf_gps->x[1] = 0;
    ekf_gps->x[3] = 0;
    ekf_gps->x[5] = 0;

    // clock bias
    ekf_gps->x[6] = 3.575261153706439e+006;

    // clock drift
    ekf_gps->x[7] = 4.549246345845814e+001;
}

static void model_gps(ekf_gps_t *ekf_gps, float SV[4][3])
{

    int i, j;

    for (j = 0; j < 8; j += 2) {
        ekf_gps->fx[j] = ekf_gps->x[j] + T * ekf_gps->x[j + 1];
        ekf_gps->fx[j + 1] = ekf_gps->x[j + 1];
    }

    for (j = 0; j < 8; ++j) {
        ekf_gps->F[j][j] = 1;
    }
    for (j = 0; j < 4; ++j) {
        ekf_gps->F[2 * j][2 * j + 1] = T;
    }
    float dx[4][3];

    for (i = 0; i < 4; ++i) {
        ekf_gps->hx[i] = 0;
        for (j = 0; j < 3; ++j) {
            float d = ekf_gps->fx[j * 2] - SV[i][j];
            dx[i][j] = d;
            ekf_gps->hx[i] += d * d;
        }
        ekf_gps->hx[i] = pow(ekf_gps->hx[i], 0.5) + ekf_gps->fx[6];
    }

    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 3; ++j) {
            ekf_gps->H[i][j * 2] = dx[i][j] / ekf_gps->hx[i];
        }
        ekf_gps->H[i][6] = 1;
    }
}

#define DATASETCOLUMNS 25

typedef struct
{
    float P11[DATASETCOLUMNS] = { -11602023.9489137, -11602700.409615,  -11603377.0261803,
                                  -11604053.7986268, -11604730.7269448, -11605407.8111641,
                                  -11606085.0512816, -11606762.44731,   -11607439.9992582,
                                  -11608117.7071296, -11608795.5709421, -11609473.590699,
                                  -11610151.7664095, -11610830.0980858, -11611508.5857306,
                                  -11612187.2293523, -11612866.0289661, -11613544.9845813,
                                  -11614224.0961969, -11614903.3638345, -11615582.7874894,
                                  -11616262.3671785, -11616942.1029125, -11617621.9946924,
                                  -11618302.042535 };
    float P12[DATASETCOLUMNS] = {
        14063117.4931116, 14060708.163762,  14058298.6961425, 14055889.0902859, 14053479.3463229,
        14051069.4642412, 14048659.444148,  14046249.2860925, 14043838.9901378, 14041428.5563671,
        14039017.9848112, 14036607.2755532, 14034196.4286552, 14031785.4441682, 14029374.3221777,
        14026963.0627483, 14024551.6659205, 14022140.1317561, 14019728.4603524, 14017316.6517278,
        14014904.7059932, 14012492.6231827, 14010080.4033526, 14007668.0465943, 14005255.5529418
    };
    float P13[DATASETCOLUMNS] = {
        18811434.3112746, 18812823.4023028, 18814212.0761809, 18815600.3328957, 18816988.1723781,
        18818375.5946411, 18819762.5996289, 18821149.1873193, 18822535.3576819, 18823921.1106749,
        18825306.4462865, 18826691.3644751, 18828075.8652108, 18829459.9484704, 18830843.6142108,
        18832226.8624009, 18833609.6930235, 18834992.1060491, 18836374.1014277, 18837755.6791551,
        18839136.8391736, 18840517.5814695, 18841897.9060167, 18843277.8127688, 18844657.3017124
    };
    float P21[DATASETCOLUMNS] = { -20853271.5736342, -20855049.9291186, -20856828.1167654,
                                  -20858606.1364935, -20860383.9882668, -20862161.6719905,
                                  -20863939.1876304, -20865716.5351111, -20867493.7143885,
                                  -20869270.7253602, -20871047.5680138, -20872824.2422708,
                                  -20874600.7480677, -20876377.0853387, -20878153.2540492,
                                  -20879929.2540921, -20881705.0854518, -20883480.7480603,
                                  -20885256.2418182, -20887031.5667084, -20888806.7226403,
                                  -20890581.7095696, -20892356.5274465, -20894131.1761796,
                                  -20895905.6557381 };
    float P22[DATASETCOLUMNS] = {
        1806977.21185816, 1805887.13065807, 1804797.28049813, 1803707.66138322, 1802618.27329107,
        1801529.116235,   1800440.19019116, 1799351.49516123, 1798263.03112756, 1797174.79810804,
        1796086.79606563, 1794999.0250037,  1793911.48491644, 1792824.17579945, 1791737.097629,
        1790650.25042617, 1789563.63415556, 1788477.24881424, 1787391.09441818, 1786305.17093311,
        1785219.47836965, 1784134.01671018, 1783048.78594032, 1781963.78607134, 1780879.01707716
    };
    float P23[DATASETCOLUMNS] = {
        16542682.1237923, 16540582.4659657, 16538482.4609004, 16536382.1086646, 16534281.4092741,
        16532180.3628133, 16530078.9692955, 16527977.2287825, 16525875.1412993, 16523772.7069394,
        16521669.9256904, 16519566.797618,  16517463.3227698, 16515359.5011966, 16513255.3329117,
        16511150.818015,  16509045.9564974, 16506940.7484123, 16504835.1938502, 16502729.2928038,
        16500623.0453534, 16498516.4515241, 16496409.5113475, 16494302.2249049, 16492194.5922053
    };
    float P31[DATASETCOLUMNS] = { -14355926.017234,  -14356344.1729806, -14356762.4791434,
                                  -14357180.9357223, -14357599.5427193, -14358018.3001422,
                                  -14358437.2079998, -14358856.2662888, -14359275.4750231,
                                  -14359694.8342019, -14360114.3438323, -14360534.0039196,
                                  -14360953.8144662, -14361373.7754756, -14361793.8869582,
                                  -14362214.1489185, -14362634.5613554, -14363055.1242758,
                                  -14363475.8376898, -14363896.7015941, -14364317.715999,
                                  -14364738.880906,  -14365160.1963243, -14365581.6622592,
                                  -14366003.2787072 };
    float P32[DATASETCOLUMNS] = {
        8650961.88410982, 8648384.47686198, 8645806.99474651, 8643229.4378562,  8640651.80627305,
        8638074.10004161, 8635496.31920119, 8632918.4638651,  8630340.53404078, 8627762.52982713,
        8625184.45127231, 8622606.29843616, 8620028.07139851, 8617449.77022857, 8614871.39495658,
        8612292.94564608, 8609714.42239723, 8607135.82525976, 8604557.15426399, 8601978.40952272,
        8599399.59106402, 8596820.69897167, 8594241.73327994, 8591662.69405015, 8589083.58139421
    };
    float P33[DATASETCOLUMNS] = {
        20736354.9805864, 20737164.3397034, 20737973.2627679, 20738781.7497543, 20739589.8006407,
        20740397.4154165, 20741204.5940731, 20742011.3365787, 20742817.6429346, 20743623.5131133,
        20744428.9471034, 20745233.94489,   20746038.5064515, 20746842.6317701, 20747646.3208399,
        20748449.5736446, 20749252.3901568, 20750054.7703644, 20750856.7142618, 20751658.2218172,
        20752459.2930258, 20753259.9278649, 20754060.1263275, 20754859.8883983, 20755659.214046
    };
    float P41[DATASETCOLUMNS] = {
        7475239.67530529, 7472917.32156931, 7470595.0720982,  7468272.92694682, 7465950.88614163,
        7463628.94979391, 7461307.1179005,  7458985.39057082, 7456663.76782936, 7454342.24973383,
        7452020.83634093, 7449699.52774197, 7447378.32393047, 7445057.2250017,  7442736.23102901,
        7440415.34201686, 7438094.55805635, 7435773.87918626, 7433453.30548462, 7431132.83699074,
        7428812.47375867, 7426492.21586256, 7424172.06332343, 7421852.01624228, 7419532.07462828
    };
    float P42[DATASETCOLUMNS] = {
        12966181.2771377, 12967714.4596339, 12969247.7736988, 12970781.2192928, 12972314.7963952,
        12973848.5049293, 12975382.344894,  12976916.3162136, 12978450.4188688, 12979984.6528181,
        12981519.0180208, 12983053.5144131, 12984588.1419961, 12986122.9007034, 12987657.7904831,
        12989192.8113291, 12990727.9631777, 12992263.2459998, 12993798.6597405, 12995334.2043704,
        12996869.8798502, 12998405.6861275, 12999941.6231851, 13001477.6909524, 13003013.8894202
    };
    float P43[DATASETCOLUMNS] = {
        21931576.7921751, 21931442.6029888, 21931307.9468087, 21931172.8236371, 21931037.233474,
        21930901.1763249, 21930764.6521883, 21930627.6610695, 21930490.2029686, 21930352.2778878,
        21930213.8858294, 21930075.0267975, 21929935.7007905, 21929795.9078129, 21929655.647868,
        21929514.9209546, 21929373.7270772, 21929232.0662369, 21929089.9384372, 21928947.3436792,
        21928804.2819651, 21928660.7532982, 21928516.7576786, 21928372.2951113, 21928227.3655957
    };
    float R1[DATASETCOLUMNS] = {
        23568206.4173783, 23568427.7909862, 23568650.0894557, 23568869.5260895, 23569094.4420916,
        23569315.4143446, 23569537.8873163, 23569760.0636344, 23569981.9083983, 23570205.8646385,
        23570427.8664544, 23570650.321976,  23570873.1090517, 23571094.6397118, 23571317.6536404,
        23571542.272989,  23571765.635922,  23571987.5330366, 23572212.1698355, 23572433.9098983,
        23572658.6513985, 23572882.7297905, 23573105.2551131, 23573329.6650593, 23573552.3125334
    };
    float R2[DATASETCOLUMNS] = {
        26183921.457745,  26184404.1127416, 26184884.7086125, 26185366.6481502, 26185845.7782029,
        26186327.8049918, 26186808.2263608, 26187289.5027905, 26187768.842246,  26188253.1899141,
        26188734.3965431, 26189215.4635703, 26189696.8272514, 26190179.3251966, 26190658.5076005,
        26191142.2270611, 26191622.8229328, 26192101.5167307, 26192584.8348365, 26193065.3609074,
        26193548.1555067, 26194030.4265996, 26194510.3070126, 26194992.9794606, 26195473.36593
    };
    float R3[DATASETCOLUMNS] = {
        24652215.2627705, 24652621.9011857, 24653025.2764103, 24653428.8435874, 24653834.853795,
        24654241.1781066, 24654645.1117385, 24655052.4830633, 24655456.8704009, 24655862.4792539,
        24656267.6169511, 24656671.8995876, 24657077.3339386, 24657484.6529132, 24657890.0872643,
        24658293.6893426, 24658699.8217026, 24659106.9487251, 24659511.3186132, 24659918.7073891,
        24660325.0840524, 24660732.8916336, 24661138.8145914, 24661542.6609733, 24661950.1370006
    };
    float R4[DATASETCOLUMNS] = {
        25606982.9330466, 25606499.4748001, 25606016.697112,  25605534.4603806, 25605048.9604585,
        25604567.3344846, 25604081.9392636, 25603599.6850818, 25603116.4885881, 25602632.6115359,
        25602148.1411763, 25601667.632016,  25601183.0395047, 25600699.4416557, 25600219.0895472,
        25599735.3346461, 25599252.7314594, 25598769.0638094, 25598287.1935317, 25597804.9916998,
        25597322.2140106, 25596841.2162436, 25596357.5136928, 25595876.9347309, 25595393.4415826
    };
} DatasetGps;

void test_backtest_gps()
{
    DatasetGps dataset;

    // Do generic EKF initialization
    ekf_gps_t ekf_gps;
    ekf_init(&ekf_gps, NUMBER_OF_STATES_GPS, NUMBER_OF_OBSERVABLES_GPS);

    // Do local initialization
    init_gps(&ekf_gps);

    // Make a place to store the data from the file and the output of the EKF
    float SV_Pos[4][3];
    float SV_Rho[4];
    float Pos_KF[25][3];

    int j, k;

    // Loop till no more data
    for (j = 0; j < 25; ++j) {

        // Load iteration of dataset
        SV_Pos[0][0] = dataset.P11[j];
        SV_Pos[0][1] = dataset.P12[j];
        SV_Pos[0][2] = dataset.P13[j];
        SV_Pos[1][0] = dataset.P21[j];
        SV_Pos[1][1] = dataset.P22[j];
        SV_Pos[1][2] = dataset.P23[j];
        SV_Pos[2][0] = dataset.P31[j];
        SV_Pos[2][1] = dataset.P32[j];
        SV_Pos[2][2] = dataset.P33[j];
        SV_Pos[3][0] = dataset.P41[j];
        SV_Pos[3][1] = dataset.P42[j];
        SV_Pos[3][2] = dataset.P43[j];

        SV_Rho[0] = dataset.R1[j];
        SV_Rho[1] = dataset.R2[j];
        SV_Rho[2] = dataset.R3[j];
        SV_Rho[3] = dataset.R4[j];

        model_gps(&ekf_gps, SV_Pos);

        ekf_step(&ekf_gps, SV_Rho);

        // grab positions, ignoring velocities
        for (k = 0; k < 3; ++k) {
            Pos_KF[j][k] = ekf_gps.x[2 * k];
        }
    }

    // Compute means of filtered positions
    float mean_Pos_KF[3] = { 0, 0, 0 };
    for (j = 0; j < 25; ++j) {
        for (k = 0; k < 3; ++k) {
            mean_Pos_KF[k] += Pos_KF[j][k];
        }
    }
    for (k = 0; k < 3; ++k) {
        mean_Pos_KF[k] /= 25;
    }

    // Debugging Dump filtered positions minus their means
    for (j = 0; j < 25; ++j) {
        // printf("%f ,%f ,%f\n", Pos_KF[j][0]-mean_Pos_KF[0], Pos_KF[j][1]-mean_Pos_KF[1],
        // Pos_KF[j][2]-mean_Pos_KF[2]); printf("%f %f %f\n", Pos_KF[j][0], Pos_KF[j][1],
        // Pos_KF[j][2]);
    }

    TEST_ASSERT_FLOAT_WITHIN(0.00001, -1.61, Pos_KF[24][0] - mean_Pos_KF[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.00001, 0.5, Pos_KF[24][1] - mean_Pos_KF[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.00001, -0.58, Pos_KF[24][2] - mean_Pos_KF[2]);
}

// TinyEKF test with SoC model

void test_ekf_init_func()
{

    // ekf_soc_t ekf_soc
    ekf_soc.P[0][0] = 5;
    ekf_init(&ekf_soc, NUMBER_OF_STATES_SOC, NUMBER_OF_OBSERVABLES_SOC);
    TEST_ASSERT_EQUAL(true, ekf_soc.P[0][0] == 0.0 && ekf_soc.Q[0][0] == 0.0
                                && ekf_soc.R[0][0] == 0.0 && ekf_soc.G[0][0] == 0.0
                                && ekf_soc.F[0][0] == 0.0 && ekf_soc.H[0][0] == 0.0);

    // TEST_ASSERT_EQUAL_FLOAT(0.0, ekf_soc.P[0][0]);
    // TEST_ASSERT_EACH_EQUAL_FLOAT(0.0,*ekf_soc.P,1); // not working yet
    // TEST_ASSERT_EQUAL_FLOAT(0,0)
}

// TODO Update to SoC
void test_ekf_step_func()
{
    ekf_gps_t ekf_gps;
    float SV_Rho[4];

    ekf_init(&ekf_gps, NUMBER_OF_STATES_GPS, NUMBER_OF_OBSERVABLES_GPS);
    ekf_gps.P[0][0] = 5;
    ekf_step(&ekf_gps, SV_Rho);
    TEST_ASSERT_EQUAL(true, ekf_gps.P[0][0] == 5.0 && ekf_gps.Q[0][0] == 0.0
                                && ekf_gps.R[0][0] == 0.0 && ekf_gps.G[0][0] == 0.0
                                && ekf_gps.F[0][0] == 0.0 && ekf_gps.H[0][0] == 0.0);

    // TEST_ASSERT_EQUAL_FLOAT(0.0, ekf_gps.P[0][0]);
    // TEST_ASSERT_EACH_EQUAL_FLOAT(0.0,&ekf_gps.P,4); // not working yet
    // TEST_ASSERT_EQUAL_FLOAT(0,0)
}

/// Test all functions implemented in bat_charger.cpp h & f and init_SoC functions

void test_clamp_func()
{
    float value, min, max, result;
    min = 0;
    max = 100000;
    value = 200000;
    result = clamp(value, min, max);
    TEST_ASSERT_FLOAT_WITHIN(0, 100000, result);
}

void test_calculate_initial_soc_func()
{
    float initial_soc, battery_voltage_mV;
    battery_voltage_mV = 12000;
    initial_soc = calculate_initial_soc(battery_voltage_mV);
    TEST_ASSERT_FLOAT_WITHIN(0, 30000, initial_soc);
}

void test_init_soc_func_should_init_with_calculated_soc()
{
    // ekf_soc_t  ekf_soc
    float P0 = 0.1;   // initial covariance of state noise  (aka process noise)
    float Q0 = 0.001; // Initial state uncertainty covariance matrix
    float R0 = 0.1;   // initial covariance of measurement noise

    // uint32_t battery_eff = 10;
    float v0 = 13000;
    float initial_soc = 0xFFFFFFFFFFFFFFFF; // forces new SoC to be calculated
    init_soc(&ekf_soc, v0, P0, Q0, R0, initial_soc);
    TEST_ASSERT_FLOAT_WITHIN(0, 100000, ekf_soc.x[0]);
}

void test_init_soc_func_should_init_with_initial_soc()
{
    // ekf_soc_t  ekf_soc
    float P0 = 0.1;   // initial covariance of state noise  (aka process noise)
    float Q0 = 0.001; // Initial state uncertainty covariance matrix
    float R0 = 0.1;   // initial covariance of measurement noise
    // uint32_t battery_eff = 10;
    float v0 = 13000;
    float initial_soc = 10.0; // forces new SoC to be calculated

    // uint32_t initial_soc = 10;
    init_soc(&ekf_soc, v0, P0, Q0, R0, initial_soc);
    TEST_ASSERT_FLOAT_WITHIN(0, 10, ekf_soc.x[0]);
}

void test_f_func()
{
    // ekf_soc_t  ekf_soc
    bool is_battery_in_float = false;
    float battery_eff = 100000;
    float battery_current_mA = 1000;
    float sample_period_milli_sec = 100;
    float battery_capacity_Ah = 50;
    f(&ekf_soc, is_battery_in_float, battery_eff, battery_current_mA, sample_period_milli_sec,
      battery_capacity_Ah);
}

void test_h_func()
{
    // ekf_soc_t  ekf_soc
    float battery_current_mA = 1000;
    h(&ekf_soc, battery_current_mA);
}

void test_should_increase_soc_no_float_leadacid_12V()
{
    int cholsl_error = 0;
    const uint32_t soc_scaled_hundred_percent = 100000;

    // Do generic EKF initialization
    // ekf_soc_t ekf_soc
    ekf_init(&ekf_soc, NUMBER_OF_STATES_SOC, NUMBER_OF_OBSERVABLES_SOC);

    // Do local initialization
    float P0 = 0.1;   // initial covariance of state noise  (aka process noise)
    float Q0 = 0.001; // Initial state uncertainty covariance matrix
    float R0 = 0.1;   // initial covariance of measurement noise
    float battery_voltage_mV[1] = {
        12500
    }; // intial Voltage measurement to calculate SoC if initial_soc is out of range
    const float battery_capacity_Ah = 50;
    float initial_soc = 50000;

    float battery_eff = 100000;
    battery_voltage_mV[0] = 12500;
    float battery_current_mA = 1000;
    float sample_period_milli_sec = 1000;
    bool is_battery_in_float = false;

    float expected_result = 50053.7539;
#ifdef DEBUG
    printf("The SoC before init_soc %f \n", ekf_soc.x[0]);
#endif

    init_soc(&ekf_soc, battery_voltage_mV[0], P0, Q0, R0, initial_soc);

#ifdef DEBUG
    printf("The SoC by init_soc %f \n", ekf_soc.x[0]);
#endif

    battery_eff = model_soc(&ekf_soc, is_battery_in_float, battery_eff, battery_current_mA,
                            sample_period_milli_sec, battery_capacity_Ah);

#ifdef DEBUG
    printf("battvol inside test %f \n", battery_voltage_mV[0]);
    printf("The SoC before ekf_step %f \n", ekf_soc.x[0]);
#endif

    cholsl_error = ekf_step(&ekf_soc, battery_voltage_mV);

#ifdef DEBUG
    if (cholsl_error != 0) {
        printf("EKFSTEP Failed %d \n", cholsl_error);
    }
    printf("The SoC before clamp %f \n", ekf_soc.x[0]);
#endif

    ekf_soc.x[0] = clamp((float)ekf_soc.x[0], 0, soc_scaled_hundred_percent);

    TEST_ASSERT_FLOAT_WITHIN(1, expected_result, ekf_soc.x[0]);
}

void test_update_soc_should_increase_soc_no_float_leadacid_12V()
{
    float expected_result = 50053.7539;
    charger.soc = 50;
    charger.port->bus->voltage = 12.500;
    charger.init_terminal(&bat_conf, &ekf_soc);

    bat_conf.float_enabled = false;
    bat_conf.nominal_capacity = 50;
    charger.port->bus->voltage = 12.500;
    charger.port->current = 1;
    charger.update_soc(&bat_conf, &ekf_soc);
#ifdef DEBUG
    printf("Soc after EKF and clamp %f\n", ekf_soc.x[0]);
#endif

    TEST_ASSERT_FLOAT_WITHIN(1, expected_result, ekf_soc.x[0]);
}

//// SoC Backtest

#define DATASETCOLUMNS_SOC 996

typedef struct
{
    float battery_voltage_mV[DATASETCOLUMNS_SOC] = {
        12230, 12240, 12240, 12230, 12230, 12240, 12230, 12230, 12520, 12540, 12560, 12570, 12580,
        12590, 12590, 12610, 12600, 12610, 12610, 12620, 12620, 12630, 12640, 12630, 12640, 12640,
        12640, 12640, 12650, 12660, 12660, 12660, 12660, 12670, 12670, 12670, 12670, 12670, 12670,
        12680, 12670, 12680, 12680, 12680, 12690, 12680, 12680, 12680, 12690, 12680, 12680, 12680,
        12690, 12690, 12690, 12690, 12690, 12690, 12690, 12690, 12700, 12690, 12690, 12690, 12700,
        12700, 12700, 12700, 12700, 12700, 12700, 12700, 12700, 12700, 12700, 12700, 12710, 12700,
        12710, 12710, 12710, 12710, 12710, 12710, 12710, 12710, 12710, 12710, 12710, 12710, 12710,
        12710, 12710, 12710, 12710, 12720, 12720, 12710, 12710, 12710, 12720, 12710, 12720, 12720,
        12720, 12720, 12720, 12730, 12720, 12720, 12720, 12720, 12730, 12720, 12720, 12720, 12720,
        12720, 12720, 12720, 12720, 12720, 12720, 12720, 12720, 12720, 12720, 12730, 12730, 12730,
        12720, 12720, 12720, 12720, 12720, 12720, 12730, 12720, 12720, 12720, 12720, 12720, 12730,
        12730, 12730, 12730, 12730, 12730, 12740, 12720, 12730, 12730, 12740, 12730, 12730, 12730,
        12730, 12730, 12730, 12730, 12730, 12730, 12740, 12730, 12730, 12730, 12730, 12730, 12730,
        12730, 12740, 12730, 12730, 12730, 12730, 12730, 12730, 12730, 12730, 12730, 12730, 12740,
        12730, 12740, 12730, 12730, 12730, 12740, 12730, 12740, 12740, 12730, 12730, 12730, 12730,
        12730, 12740, 12730, 12740, 12730, 12740, 12740, 12740, 12730, 12740, 12740, 12740, 12740,
        12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740, 12750, 12740, 12740, 12740, 12740,
        12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740, 12740,
        12740, 12740, 12740, 12740, 12740, 12740, 12750, 12740, 12740, 12740, 12740, 12750, 12740,
        12750, 12740, 12740, 12750, 12740, 12740, 12740, 12740, 12750, 12750, 12740, 12750, 12740,
        12750, 12740, 12740, 12750, 12740, 12740, 12750, 12750, 12740, 12740, 12750, 12740, 12750,
        12750, 12750, 12750, 12750, 12750, 12740, 12750, 12740, 12750, 12740, 12750, 12760, 12750,
        12740, 12750, 12750, 12760, 12750, 12760, 12750, 12750, 12750, 12750, 12760, 12750, 12750,
        12760, 12750, 12760, 12750, 12750, 12750, 12750, 12750, 12750, 12750, 12750, 12750, 12750,
        12750, 12750, 12750, 12750, 12750, 12750, 12750, 12760, 12750, 12760, 12760, 12760, 12750,
        12760, 12750, 12750, 12750, 12750, 12760, 12760, 12750, 12750, 12750, 12750, 12750, 12760,
        12750, 12750, 12750, 12750, 12750, 12750, 12750, 12750, 12750, 12760, 12750, 12760, 12750,
        12760, 12750, 12750, 12760, 12750, 12750, 12750, 12760, 12750, 12750, 12750, 12750, 12750,
        12750, 12750, 12750, 12770, 12750, 12760, 12760, 12750, 12750, 12750, 12750, 12760, 12750,
        12750, 12760, 12750, 12750, 12750, 12750, 12750, 12760, 12750, 12750, 12750, 12750, 12750,
        12760, 12750, 12750, 12760, 12760, 12760, 12760, 12760, 12760, 12750, 12760, 12750, 12750,
        12750, 12760, 12760, 12750, 12750, 12760, 12750, 12760, 12750, 12760, 12770, 12750, 12760,
        12750, 12770, 12750, 12750, 12760, 12760, 12760, 12750, 12750, 12760, 12750, 12750, 12760,
        12750, 12760, 12770, 12750, 12770, 12770, 12750, 12770, 12760, 12760, 12760, 12760, 12760,
        12770, 12760, 12770, 12750, 12760, 12760, 12760, 12760, 12760, 12750, 12750, 12760, 12760,
        12760, 12760, 12760, 12760, 12760, 12770, 12760, 12760, 12760, 12760, 12760, 12760, 12760,
        12760, 12770, 12760, 12760, 12770, 12760, 12760, 12750, 12760, 12770, 12760, 12760, 12760,
        12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12770, 12760, 12760, 12770,
        12760, 12770, 12760, 12770, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760,
        12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760,
        12760, 12760, 12770, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12760, 12770, 12760,
        12770, 12770, 12760, 12760, 12770, 12760, 12760, 12760, 12770, 12760, 12770, 12760, 12770,
        12760, 12770, 12770, 12770, 12760, 12760, 12770, 12760, 12760, 12760, 12770, 12770, 12760,
        12770, 12770, 12760, 12770, 12760, 12770, 12770, 12760, 12770, 12770, 12770, 12770, 12770,
        12770, 12770, 12760, 12770, 12770, 12770, 12770, 12770, 12770, 12760, 12770, 12780, 12770,
        12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12760, 12760,
        12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770,
        12770, 12770, 12770, 12770, 12780, 12770, 12770, 12770, 12770, 12780, 12770, 12770, 12770,
        12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770,
        12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12780, 12770, 12780,
        12770, 12770, 12770, 12780, 12770, 12780, 12780, 12770, 12780, 12770, 12770, 12770, 12770,
        12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770,
        12780, 12770, 12780, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12780, 12770, 12770,
        12770, 12770, 12780, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770,
        12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12770, 12780,
        12770, 12780, 12770, 12770, 12770, 12780, 12780, 12770, 12770, 12770, 12770, 12770, 12770,
        12770, 12780, 12770, 12780, 12770, 12770, 12780, 12770, 12770, 12770, 12770, 12780, 12770,
        12770, 12770, 12770, 12770, 12770, 12780, 12770, 12780, 12780, 12770, 12770, 12770, 12770,
        12770, 12780, 12770, 12770, 12770, 12780, 12780, 12780, 12770, 12770, 12770, 12780, 12780,
        12770, 12770, 12780, 12770, 12770, 12770, 12770, 12780, 12770, 12780, 12780, 12780, 12770,
        12770, 12780, 12770, 12770, 12780, 12770, 12770, 12780, 12780, 12770, 12770, 12780, 12780,
        12770, 12780, 12780, 12770, 12770, 12770, 12780, 12780, 12780, 12780, 12780, 12770, 12770,
        12780, 12780, 12780, 12780, 12780, 12780, 12780, 12790, 12780, 12790, 12780, 12780, 12770,
        12770, 12780, 12770, 12770, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780,
        12780, 12790, 12790, 12780, 12780, 12780, 12790, 12780, 12780, 12780, 12780, 12780, 12780,
        12780, 12780, 12780, 12790, 12780, 12770, 12790, 12780, 12790, 12780, 12790, 12780, 12780,
        12790, 12780, 12790, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780,
        12780, 12780, 12780, 12780, 12790, 12780, 12780, 12780, 12790, 12790, 12780, 12770, 12780,
        12780, 12780, 12770, 12780, 12770, 12770, 12780, 12780, 12780, 12780, 12780, 12780, 12780,
        12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12770, 12780, 12790, 12780, 12790,
        12790, 12790, 12790, 12780, 12780, 12790, 12780, 12780, 12790, 12790, 12780, 12780, 12780,
        12780, 12780, 12780, 12780, 12780, 12780, 12790, 12780, 12780, 12780, 12790, 12780, 12780,
        12790, 12790, 12790, 12790, 12780, 12790, 12780, 12780, 12780, 12780, 12780, 12780, 12780,
        12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12790, 12780, 12780, 12780,
        12790, 12780, 12780, 12790, 12780, 12780, 12780, 12790, 12780, 12790, 12780, 12790, 12790,
        12780, 12780, 12790, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12780, 12790, 12790,
        12780, 12780, 12780, 12780, 12790, 12780, 12780, 12790
    };
    float battery_current_mA[DATASETCOLUMNS_SOC] = {
        -3000, -3000, -3000, -3000, -3000, -3000, -3000, -2990, 10, 10, 10, 10, 10, 10, 10, 0,  0,
        10,    10,    0,     10,    10,    10,    10,    0,     0,  10, 10, 10, 10, 10, 10, 10, 10,
        10,    0,     10,    10,    10,    10,    0,     10,    0,  0,  10, 10, 0,  0,  10, 10, 0,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 0,  0,
        10,    10,    0,     0,     10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 0,  0,  10,
        10,    10,    10,    0,     10,    10,    0,     0,     0,  10, 10, 10, 10, 10, 10, 0,  10,
        10,    10,    0,     0,     0,     10,    10,    10,    10, 10, 10, 0,  10, 0,  10, 10, 10,
        10,    10,    10,    10,    10,    10,    0,     10,    10, 10, 10, 0,  10, 10, 10, 10, 10,
        10,    0,     10,    10,    0,     255.6, 10,    10,    10, 0,  10, 10, 10, 10, 10, 10, 10,
        0,     10,    10,    10,    0,     10,    0,     10,    0,  10, 10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    10,    10,    0,     10,    0,     10, 0,  10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    10,    10,    10,    0,     10,    10, 10, 0,  10, 10, 10, 0,  10, 10,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    10,    10,    0,     10,    0,     0,  0,  10, 10, 10, 10, 10, 0,  10,
        10,    10,    10,    10,    10,    10,    10,    10,    0,  10, 0,  10, 10, 10, 10, 0,  0,
        10,    10,    10,    10,    10,    10,    10,    0,     10, 10, 10, 10, 10, 0,  10, 10, 10,
        10,    10,    10,    10,    10,    10,    10,    0,     10, 0,  10, 10, 10, 10, 10, 10, 10,
        10,    0,     10,    10,    10,    10,    0,     10,    0,  10, 10, 0,  10, 10, 10, 10, 10,
        0,     10,    20,    10,    10,    10,    10,    10,    0,  10, 0,  10, 10, 10, 10, 10, 10,
        10,    0,     10,    10,    10,    0,     10,    10,    10, 0,  10, 10, 10, 0,  10, 10, 10,
        0,     0,     10,    10,    0,     10,    0,     10,    10, 10, 10, 10, 10, 0,  0,  10, 10,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    0,     0,     10,    10,    10,    10, 0,  10, 0,  0,  10, 10, 10, 10,
        10,    10,    10,    10,    10,    0,     10,    0,     10, 10, 10, 10, 10, 10, 10, 0,  10,
        10,    10,    0,     0,     10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        0,     10,    10,    0,     0,     10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 0,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        0,     10,    10,    0,     0,     10,    10,    10,    10, 10, 10, 0,  10, 10, 10, 10, 10,
        10,    10,    10,    10,    0,     0,     10,    10,    0,  10, 0,  10, 10, 10, 10, 10, 10,
        10,    10,    0,     10,    10,    0,     10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        10,    10,    0,     10,    10,    10,    0,     10,    10, 10, 10, 10, 0,  10, 10, 0,  10,
        0,     10,    10,    0,     10,    10,    10,    10,    10, 10, 10, 10, 10, 20, 10, 10, 0,
        10,    0,     0,     0,     10,    0,     10,    0,     10, 10, 10, 10, 0,  0,  10, 0,  10,
        10,    10,    0,     10,    10,    10,    10,    10,    10, 10, 10, 0,  10, 0,  10, 10, 0,
        0,     0,     10,    10,    0,     10,    10,    0,     10, 10, 10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 10, 0,  0,  10, 10, 10, 10, 10,
        10,    0,     10,    10,    10,    10,    0,     10,    0,  10, 10, 10, 10, 10, 10, 10, 10,
        10,    0,     10,    10,    10,    10,    0,     10,    0,  0,  0,  10, 10, 10, 10, 0,  0,
        10,    10,    10,    10,    0,     10,    10,    10,    0,  10, 10, 10, 10, 10, 10, 10, 10,
        20,    10,    10,    10,    10,    10,    0,     10,    0,  10, 10, 0,  0,  0,  10, 0,  0,
        0,     10,    0,     10,    10,    10,    10,    0,     0,  10, 10, 10, 10, 10, 10, 0,  10,
        10,    0,     10,    0,     0,     0,     10,    0,     10, 0,  10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        0,     10,    10,    10,    0,     0,     10,    10,    10, 10, 10, 10, 10, 0,  0,  10, 10,
        10,    10,    10,    10,    0,     10,    10,    10,    10, 10, 10, 10, 10, 10, 10, 0,  10,
        10,    10,    10,    10,    10,    10,    10,    10,    0,  10, 10, 10, 10, 0,  10, 10, 0,
        10,    10,    0,     0,     10,    10,    10,    10,    10, 10, 10, 0,  10, 10, 0,  10, 10,
        10,    10,    10,    0,     10,    10,    0,     10,    0,  10, 0,  10, 0,  10, 10, 10, 10,
        0,     10,    10,    10,    10,    10,    0,     0,     10, 10, 10, 10, 10, 10, 0,  10, 10,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 0,  10, 10, 10, 10, 10, 10, 10,
        10,    10,    10,    10,    10,    10,    10,    0,     10, 0,  10, 10, 10, 10, 10, 10, 0,
        0,     0,     0,     0,     10,    10,    0,     0,     10, 10, 10, 10, 10, 10, 0,  0,  0,
        10,    0,     10,    10,    10,    10,    10,    0,     10, 10, 10, 10, 10, 10, 0,  10, 0,
        10,    10,    10,    10,    0,     0,     10,    0,     10, 10, 10, 10, 0,  10, 10, 10, 10,
        10,    10,    0,     10,    10,    10,    10,    10,    0,  10, 0,  10, 0,  10, 10, 10, 0,
        0,     10,    10,    10,    10,    0,     10,    10,    10, 10, 10, 10, 10, 10, 10, 10, 10,
        10,    0,     0,     10,    10,    10,    10,    0,     10, 10, 10, 0,  10, 0,  10, 0,  0,
        10,    10,    10,    10,    10,    10,    10,    10,    10, 0,  10, 10, 10, 10, 10, 10, 0,
        10,    10,    0,     10,    10,    10,    0,     10,    10, 10
    };
    bool is_battery_in_float[DATASETCOLUMNS_SOC] = {
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false
    };
    float sample_period_milli_sec[DATASETCOLUMNS_SOC] = {
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
        1000, 1000, 1000, 1000, 1000, 1000
    };
} dataset_SoC_t;

void test_backtest_SoC()
{
    dataset_SoC_t dataset;
    int cholsl_error;
    // float batteryVoltage = 13.01; //batteryvoltage measurement TODO delete

    // Do generic EKF initialization
    // ekf_soc_t ekf_soc
    ekf_init(&ekf_soc, NUMBER_OF_STATES_SOC, NUMBER_OF_OBSERVABLES_SOC);

    // Do local initialization
    float P0 = 0.1;   // initial covariance of state noise  (aka process noise)
    float Q0 = 0.001; // Initial state uncertainty covariance matrix
    float R0 = 0.1;   // initial covariance of measurement noise
    float initial_soc = 0xFFFFFFFFFFF;
    init_soc(&ekf_soc, dataset.battery_voltage_mV[0], P0, Q0, R0, initial_soc);
    float battery_capacity_Ah = 12;
    float battery_eff = 85000;
    float expected_result = 71491.4609;
    const uint32_t soc_scaled_hundred_percent = 100000; // 100% charge = 100000

    int j;

    // Loop till no more data
    for (j = 1; j < DATASETCOLUMNS_SOC; ++j) {

        // $\hat{x}_k = f(\hat{x}_{k-1})$
        battery_eff =
            f(&ekf_soc, dataset.is_battery_in_float[j], battery_eff, dataset.battery_current_mA[j],
              dataset.sample_period_milli_sec[j], battery_capacity_Ah);
        // update measurable (voltage) based on predicted state (SOC)
        h(&ekf_soc, dataset.battery_current_mA[j]);
#ifdef DEBUG
        printf("battvol inside test %f \n", dataset.battery_voltage_mV[j]);
#endif
        cholsl_error = ekf_step(&ekf_soc, &dataset.battery_voltage_mV[j]);
#ifdef DEBUG
        if (cholsl_error != 0)
            printf("EKFSTEP Failed");
#endif
        ekf_soc.x[0] = clamp((float)ekf_soc.x[0], 0, soc_scaled_hundred_percent);
#ifdef DEBUG
        printf("\n\n\nThe SoC after clamp %f \n\n\n", ekf_soc.x[0]);
#endif
    }

    TEST_ASSERT_FLOAT_WITHIN(1, expected_result, ekf_soc.x[0]);
}

int kalman_soc_tests()
{

    UNITY_BEGIN();

    // RUN_TEST(test_backtest_gps);
    RUN_TEST(test_ekf_init_func);
    RUN_TEST(test_ekf_step_func);
    RUN_TEST(test_clamp_func);
    RUN_TEST(test_calculate_initial_soc_func);
    // RUN_TEST(test_init_soc_func_should_init_with_initial_soc);
    // RUN_TEST(test_init_soc_func_should_init_with_calculated_soc);
    // RUN_TEST(test_f_func);
    // RUN_TEST(test_h_func);
    RUN_TEST(test_should_increase_soc_no_float_leadacid_12V);
    RUN_TEST(test_update_soc_should_increase_soc_no_float_leadacid_12V);

    RUN_TEST(test_backtest_SoC);

    return UNITY_END();
}
