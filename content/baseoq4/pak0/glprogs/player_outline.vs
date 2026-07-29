#version 110

// x: outline width in viewport pixels
// y: clip-space units per horizontal pixel ( 2 / viewport width )
// z: clip-space units per vertical pixel ( 2 / viewport height )
// w: unused
uniform vec4 uOutlineParams;

void main() {
	vec4 clipPosition = gl_ModelViewProjectionMatrix * gl_Vertex;
	vec3 eyeNormal = gl_NormalMatrix * gl_Normal;

	// Project the eye-space normal to find the screen direction that pushes this
	// vertex away from the silhouette. Vertices facing the camera barely move,
	// vertices on the silhouette move the full width, so the shell only widens
	// where the outline is actually visible.
	vec2 screenNormal = ( gl_ProjectionMatrix * vec4( eyeNormal, 0.0 ) ).xy;
	float screenLength = length( screenNormal );
	if ( screenLength > 0.0001 ) {
		// Scaling by w cancels the perspective divide, so the offset stays a
		// constant pixel count at any distance from the camera.
		vec2 offset = ( screenNormal / screenLength ) * vec2( uOutlineParams.y, uOutlineParams.z );
		clipPosition.xy += offset * uOutlineParams.x * max( clipPosition.w, 0.001 );
	}

	gl_Position = clipPosition;
}
