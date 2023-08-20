/*
 * TinyEKF: Extended Kalman Filter for embedded processors
 *
 * Copyright (C) 2015 Simon D. Levy
 *
 * MIT License
 */

/**
 * @brief
 *
 * @param v Battery voltage to calculate the inital SoC for the EKF
 * @param n Matrix dimension n columns
 * @param m Matrix dimension m rows
 */
void ekf_init(void *v, int n, int m);

/**
 * @brief
 *
 * @param v Pointer to struct containing EKF Data.
 * @param z Pointer to voltage measurement for the iteration
 * @return int Return success, dependend on numercial stability.
 */
int ekf_step(void *v, float *z);
