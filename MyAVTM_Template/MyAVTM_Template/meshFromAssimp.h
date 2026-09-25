bool Import3DFromFile(const std::string& pFile, Assimp::Importer& importer, const aiScene*& sc, float& scaleFactor);
void createMyMeshFromAssimp(const aiScene*& sc, Renderer* render);
void get_bounding_box(const aiScene* scene, aiVector3D* min, aiVector3D* max);