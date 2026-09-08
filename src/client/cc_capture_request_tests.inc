/* Parse capture requests before opening a window or loading a save. */
static int RunCaptureRequestRegression(void)
{
    CcCaptureRequest request = {0};
    char *normal[] = {"crownless"};
    if (!CcCaptureRequestParse(1, normal, &request) || request.capture)
        return ClientRegressionFailure("Normal startup must select play mode.");
    char *plain[] = {"crownless", "--capture", "proof.png"};
    if (!CcCaptureRequestParse(3, plain, &request) || !request.capture ||
        strcmp(request.capture_path, "proof.png") != 0)
        return ClientRegressionFailure("A plain capture must retain its output path.");
    char *ux[] = {"crownless", "--capture-ux", "14", "ux.png", "1040", "2"};
    if (!CcCaptureRequestParse(6, ux, &request) || !request.capture_ux ||
        !request.capture || request.capture_ux_view != 14 ||
        strcmp(request.capture_path, "ux.png") != 0)
        return ClientRegressionFailure("UX capture must retain its view and path.");
    ux[2] = "15";
    if (CcCaptureRequestParse(6, ux, &request))
        return ClientRegressionFailure("UX capture must enforce its view range.");
    char *storybook[] = {"crownless", "--capture-storybook", "0.25", "0.75", "story.png"};
    if (!CcCaptureRequestParse(5, storybook, &request) || !request.capture ||
        !request.capture_storybook || request.storybook_progress != 0.25f ||
        request.storybook_blend != 0.75f || strcmp(request.capture_path, "story.png") != 0)
        return ClientRegressionFailure("Storybook capture must retain both scene settings.");
    storybook[2] = "nan";
    if (CcCaptureRequestParse(5, storybook, &request))
        return ClientRegressionFailure("Storybook progress must be finite.");
    char *arrival[] = {"crownless", "--capture-road-arrival", "0.5", "arrival.png"};
    if (!CcCaptureRequestParse(4, arrival, &request) || !request.capture ||
        !request.capture_road_arrival || !request.capture_road_zoom ||
        request.capture_road_zoom_weight != 0.5f ||
        strcmp(request.capture_path, "arrival.png") != 0)
        return ClientRegressionFailure("Road arrival must retain the zoom capture alias.");
    if (!CcCaptureRequestParse(1, normal, &request) || request.capture ||
        request.capture_road_arrival || request.capture_storybook)
        return ClientRegressionFailure("Each parse must reset the previous request.");
    (void)puts("PASS capture requests: play, paths, views, scene settings, and ranges");
    return 0;
}
