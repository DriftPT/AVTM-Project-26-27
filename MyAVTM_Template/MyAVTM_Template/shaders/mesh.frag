#version 430

struct Materials {
	vec4 diffuse;
	vec4 ambient;
	vec4 specular;
	vec4 emissive;
	float shininess;
	int texCount;
};

in Data {
	vec3 normal;
	vec3 eye;
	vec3 lightDir;
	vec2 tex_coord;
	vec3 fragPos;
} DataIn;

uniform Materials mat;

uniform sampler2D texmap;
uniform sampler2D texmap1;
uniform sampler2D texmap2;

uniform int texMode;

// Global Illumination - Directional Light
uniform bool globalLightOn;
uniform vec4 globalLightPos;

// Spotlight Illumination
uniform bool spotlight_mode;
uniform vec4 spotLightPos0;
uniform vec4 spotLightPos1;
uniform vec4 coneDir;
uniform float spotCosCutOff;

// 6 Candles Lights - Point Lights
uniform bool candlesLightOn;
uniform vec4 candlesLightPos0, candlesLightPos1, candlesLightPos2, candlesLightPos3, candlesLightPos4, candlesLightPos5;

uniform bool fogEnabled;
uniform vec3 fogColor;
uniform int depthFog;

out vec4 colorOut;

// Calculate the global illumination
vec3 calculateGlobalLight(vec3 n, vec3 e, out vec3 specular)
{
    specular = vec3(0.0);

    if (!globalLightOn)
        return vec3(0.0);

    vec3 l = normalize(globalLightPos.xyz);

    // Diffuse light intensity
    float intensity = max(dot(n, l), 0.0);

    // Specular
    if (intensity > 0.0) {
        vec3 h = normalize(l + e);
        float intSpec = max(dot(h, n), 0.0);
        specular = mat.specular.rgb * pow(intSpec, mat.shininess);
    }

    return vec3(intensity);
}

//Calculate the spotlight illumination
vec3 calculateSpotLight(vec4 lightPos, vec3 spotDirection, vec3 n, vec3 e, out vec3 specular)
{
    specular = vec3(0.0);

    if (!spotlight_mode)
        return vec3(0.0);

    float spotExp = 60.0;

    vec3 lightVector = lightPos.xyz - DataIn.fragPos;
    vec3 l = normalize(lightVector);
    vec3 sd = normalize(spotDirection);

    float spotCos = dot(-l, sd);

    if (spotCos <= spotCosCutOff)
        return vec3(0.0);

    float att = pow(spotCos, spotExp);
    float intensity = max(dot(n, l), 0.0) * att;

    if (intensity <= 0.0)
        return vec3(0.0);

    // Specular
    vec3 h = normalize(l + e);
    float intSpec = max(dot(h, n), 0.0);
    specular = mat.specular.rgb * pow(intSpec, mat.shininess) * att;

    return vec3(intensity);
}

// Calculate the candles illumination
vec3 calculateCandleLight(vec4 lightPos, vec3 n, vec3 e, out vec3 specular)
{
    specular = vec3(0.0);

    vec3 lightVector = lightPos.xyz - DataIn.fragPos;
    vec3 l = normalize(lightVector);

    float intensity = max(dot(n, l), 0.0);

    if (intensity <= 0.0)
        return vec3(0.0);

    // Specular
    vec3 h = normalize(l + e);
    float intSpec = max(dot(h, n), 0.0);
    specular = mat.specular.rgb * pow(intSpec, mat.shininess);

    return vec3(intensity);
}

void main() {
	vec4 texel, texel1;

	vec3 n = normalize(DataIn.normal);
	vec3 e = normalize(DataIn.eye);

	//Global Light
	vec3 globalSpecular; 
	vec3 globalLight = calculateGlobalLight(n, e, globalSpecular);

	// Spotlight
	vec3 spotlight = vec3(0.0);
	vec3 spotlightSpecular = vec3(0.0);

	if (spotlight_mode)	{
		vec3 spec0;
		vec3 spec1;

		spotlight += calculateSpotLight(spotLightPos0, coneDir.xyz, n, e, spec0);
		spotlight += calculateSpotLight(spotLightPos1, coneDir.xyz, n, e, spec1);
		spotlightSpecular = spec0 + spec1;
	}

	// Candle Lights
	vec3 candleLights = vec3(0.0);
	vec3 candleSpecular = vec3(0.0);

	if(candlesLightOn){
		vec3 spec;

		candleLights += calculateCandleLight(candlesLightPos0, n, e, spec);
		candleSpecular += spec;

		candleLights += calculateCandleLight(candlesLightPos1, n, e, spec);
		candleSpecular += spec;

		candleLights += calculateCandleLight(candlesLightPos2, n, e, spec);
		candleSpecular += spec;

		candleLights += calculateCandleLight(candlesLightPos3, n, e, spec);
		candleSpecular += spec;

		candleLights += calculateCandleLight(candlesLightPos4, n, e, spec);
		candleSpecular += spec;

		candleLights += calculateCandleLight(candlesLightPos5, n, e, spec);
		candleSpecular += spec;
	}

	// Final Light
	vec3 baseColor;
	vec3 lightIntensity = globalLight + spotlight + candleLights;
	vec3 totalSpecular = globalSpecular + spotlightSpecular + candleSpecular;

	// Textures
	if(texMode == 0) //no texturing
		 baseColor = mat.ambient.rgb + lightIntensity * mat.diffuse.rgb;

	else if(texMode == 1) // modulate diffuse color with texel color
	{
		texel = texture(texmap2, DataIn.tex_coord);  // texel from lighwood.tga
		baseColor = mat.ambient.rgb * texel.rgb + lightIntensity * mat.diffuse.rgb * texel.rgb;
	}
	else if (texMode == 2) // diffuse color is replaced by texel color
	{
		texel = texture(texmap, DataIn.tex_coord);  // texel from stone.tga
		baseColor = 0.07 * texel.rgb + lightIntensity * texel.rgb;
	}
	else if (texMode == 3) // multitexturing
	{
		texel = texture(texmap2, DataIn.tex_coord);  // texel from lighwood.tga
		texel1 = texture(texmap1, DataIn.tex_coord);  // texel from checker.tga
		vec3 textureColor = texel.rgb * texel1.rgb;
		baseColor = 0.07 * textureColor + lightIntensity * textureColor;
	}
	else if (texMode == 4) // constant shading (sombras planares)
	{
		baseColor = vec3(0.5, 0.5, 0.5);
	}
	else // valor de texMode inesperado - mostra magenta para ser óbvio que há um bug
	{
		baseColor = vec3(1.0, 0.0, 1.0);
	}

	vec3 finalColor = (texMode == 4) ? baseColor : baseColor + totalSpecular;
	colorOut = vec4(finalColor, mat.diffuse.a);

	if (fogEnabled) {
		float dist;

		if (depthFog == 0)
		{
			// Plane based
			dist = abs(DataIn.eye.z);
		}
		else
		{
			// Range based
			dist = length(DataIn.eye);
		}

		float fogAmount = exp(-dist * 0.025);
		if (texMode == 4)
			colorOut.rgb = mix(vec3(1.0), colorOut.rgb, fogAmount); // sombra desvanece para "neutro", não para o céu
		else
			colorOut.rgb = mix(fogColor, colorOut.rgb, fogAmount);
	}
}