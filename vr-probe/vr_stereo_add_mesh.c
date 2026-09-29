/* X-Wing mesh: create independent V10 vertex/index buffers from AeronSceneMesh
 * and draw using the existing V10 pipeline (shadercross VS+FS, clip space). */
int VrStereo_CreateMeshBuffers(AeronSceneMesh *mesh);
void VrStereo_DrawMesh(AeronCommandBuffer *cmd);