//
// Created by Göksu Güvendiren on 2019-05-14.
//

#include "Scene.hpp"


void Scene::buildBVH() {
    printf(" - Generating BVH...\n\n");
    this->bvh = new BVHAccel(objects, 1, BVHAccel::SplitMethod::NAIVE);
}

Intersection Scene::intersect(const Ray &ray) const
{
    return this->bvh->Intersect(ray);
}

void Scene::sampleLight(Intersection &pos, float &pdf) const
{
    float emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        if (objects[k]->hasEmit()){
            emit_area_sum += objects[k]->getArea();
        }
    }
    float p = get_random_float() * emit_area_sum;
    emit_area_sum = 0;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        if (objects[k]->hasEmit()){
            emit_area_sum += objects[k]->getArea();
            if (p <= emit_area_sum){
                objects[k]->Sample(pos, pdf);
                break;
            }
        }
    }
}

bool Scene::trace(
        const Ray &ray,
        const std::vector<Object*> &objects,
        float &tNear, uint32_t &index, Object **hitObject)
{
    *hitObject = nullptr;
    for (uint32_t k = 0; k < objects.size(); ++k) {
        float tNearK = kInfinity;
        uint32_t indexK;
        Vector2f uvK;
        if (objects[k]->intersect(ray, tNearK, indexK) && tNearK < tNear) {
            *hitObject = objects[k];
            tNear = tNearK;
            index = indexK;
        }
    }


    return (*hitObject != nullptr);
}

// Implementation of Path Tracing
Vector3f Scene::castRay(const Ray &ray, int depth) const
{
    // TO DO Implement Path Tracing Algorithm here
    Intersection inter = intersect(ray);
    if (inter.happened)
        return shade(inter, ray);
    else
        return this->backgroundColor;
}

Vector3f Scene::shade(const Intersection & inter, const Ray &ray) const
{
    Material *m = inter.m;
    Vector3f normal = inter.normal;

    // inter is on the light
    if (m->hasEmission())
        return m->getEmission();
    
    Vector3f L_dir;
    Vector3f L_indir;

    Intersection pos;
    float pdf;
    sampleLight(pos, pdf);

    auto ws = (pos.coords - inter.coords).normalized();
    bool notBlocked = (intersect(Ray(inter.coords, ws)).coords - pos.coords).norm() < 5e-4f;
    bool hasDirectLight = false;

    if (notBlocked)
    {
        auto dist = pow((pos.coords - inter.coords).norm(), 2);
        double cos1 = std::max(0.0f, dotProduct(ws, normal));
        double cos2 = std::max(0.0f, dotProduct(-ws, pos.normal));
        L_dir = pos.emit * m->eval(ws, -ray.direction, normal) * cos1 * cos2 / pdf / dist;
        
        // due to sometimes we won't get the correct ray to reflect light in the specular surface.
        // thus, we let the ray to choose it's own directon.
        hasDirectLight = L_dir.norm() > 0.1; 
    }

    // indirect light
    if (get_random_float() <= RussianRoulette) {
        auto wi = m->sample(ray.direction, normal);

        Ray ray2(inter.coords, wi);
        Intersection pos2 = intersect(ray2);

        if (pos2.happened && (!hasDirectLight || !pos2.m->hasEmission())) {
            double cos1 = std::max(0.0f, dotProduct(wi, normal));
            L_indir = shade(pos2, ray2) * m->eval(wi, -ray.direction, normal) * cos1 / std::max(m->pdf(wi, -ray.direction, normal), 0.0001f) / RussianRoulette;
        }
    }

    return Vector3f::Max(Vector3f::Min(L_dir + L_indir, Vector3f(1.0f)), Vector3f(0.0f));
}