// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import groovy.util.slurpersupport.GPathResult
import org.gradle.api.Project
import org.gradle.api.artifacts.ResolvedArtifact
import org.gradle.api.artifacts.ResolvedConfiguration
import org.gradle.api.artifacts.ResolvedDependency
import org.gradle.api.artifacts.ResolvedModuleVersion
import org.gradle.api.artifacts.component.ComponentIdentifier
import org.gradle.api.artifacts.result.ResolvedArtifactResult
import org.gradle.maven.MavenModule
import org.gradle.maven.MavenPomArtifact

/**
 * Parses the project dependencies and generates a graph of
 * {@link ChromiumDepGraph.DependencyDescription} objects to make the data manipulation easier.
 */
class ChromiumDepGraph {
    final def dependencies = new HashMap<String, DependencyDescription>()

    // Some libraries don't properly fill their POM with the appropriate licensing information.
    // It is provided here from manual lookups.
    final def FALLBACK_PROPERTIES = [
        'com_google_googlejavaformat_google_java_format': new DependencyDescription(
          url: "https://github.com/google/google-java-format",
          licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
          licenseName: "Apache 2.0"),
        'com_google_guava_guava': new DependencyDescription(
          url: "https://github.com/google/guava",
          licenseUrl: "https://www.apache.org/licenses/LICENSE-2.0.txt",
          licenseName: "Apache 2.0"),
        'org_codehaus_mojo_animal_sniffer_annotations': new DependencyDescription(
          url: "http://www.mojohaus.org/animal-sniffer/animal-sniffer-annotations/",
          licenseUrl: "https://opensource.org/licenses/mit-license.php",
          licenseName: "MIT"),
        'org_checkerframework_checker_compat_qual' :new DependencyDescription(
          licenseUrl: "https://github.com/typetools/checker-framework/blob/master/LICENSE.txt",
          licenseName: "GPL v2 (with the classpath exception)"),
    ]

    Project project

    void collectDependencies() {
        def androidConfig = project.configurations.getByName('compile').resolvedConfiguration
        List<String> topLevelIds = []
        Set<ResolvedConfiguration> deps = []
        deps += androidConfig.firstLevelModuleDependencies
        deps += project.configurations.getByName('annotationProcessor').resolvedConfiguration
                .firstLevelModuleDependencies

        deps.each { dependency ->
            topLevelIds.add(makeModuleId(dependency.module))
            collectDependenciesInternal(dependency)
        }

        topLevelIds.each { id -> dependencies.get(id).visible = true }

        androidConfig.resolvedArtifacts.each { artifact ->
            def dep = dependencies.get(makeModuleId(artifact))
            assert dep != null : "No dependency collected for artifact ${artifact.name}"

            dep.supportsAndroid = true
        }
    }

    private ResolvedArtifactResult getPomFromArtifact(ComponentIdentifier componentId) {
        def component = project.dependencies.createArtifactResolutionQuery()
                .forComponents(componentId)
                .withArtifacts(MavenModule, MavenPomArtifact)
                .execute()
                .resolvedComponents[0]
        return component.getArtifacts(MavenPomArtifact)[0]
    }

    private void collectDependenciesInternal(ResolvedDependency dependency) {
        def id = makeModuleId(dependency.module)
        if (dependencies.containsKey(id)) {
            if (dependencies.get(id).version <= dependency.module.id.version) return
        }

        def childModules = []
        dependency.children.each { childDependency ->
            childModules += makeModuleId(childDependency.module)
        }

        if (dependency.getModuleArtifacts().size() != 1) {
            throw new IllegalStateException("The dependency ${id} does not have exactly one " +
                                            "artifact: ${dependency.getModuleArtifacts()}")
        }
        def artifact = dependency.getModuleArtifacts()[0]
        if (artifact.extension != 'jar' && artifact.extension != 'aar') {
            throw new IllegalStateException("Type ${artifact.extension} of ${id} not supported.")
        }

        dependencies.put(id, buildDepDescription(id, dependency, artifact, childModules))
        dependency.children.each {
            childDependency -> collectDependenciesInternal(childDependency)
        }
    }

    static String makeModuleId(ResolvedModuleVersion module) {
        // Does not include version because by default the resolution strategy for gradle is to use
        // the newest version among the required ones. We want to be able to match it in the
        // BUILD.gn file.
        return sanitize("${module.id.group}_${module.id.name}")
    }

    static String makeModuleId(ResolvedArtifact artifact) {
        // Does not include version because by default the resolution strategy for gradle is to use
        // the newest version among the required ones. We want to be able to match it in the
        // BUILD.gn file.
        def componentId = artifact.id.componentIdentifier
        return sanitize("${componentId.group}_${componentId.module}")
    }

    private static String sanitize(String input) {
        return input.replaceAll("[:.-]", "_")
    }

    private buildDepDescription(String id, ResolvedDependency dependency, ResolvedArtifact artifact,
                                List<String> childModules) {
        def pom = getPomFromArtifact(artifact.id.componentIdentifier).file
        def pomContent = new XmlSlurper(false, false).parse(pom)
        String licenseName
        String licenseUrl
        (licenseName, licenseUrl) = resolveLicenseInformation(id, pomContent)

        // Get rid of irrelevant indent that might be present in the XML file.
        def description = pomContent.description?.text()?.trim()?.replaceAll(/\s+/, " ")

        return customizeDep(new DependencyDescription(
                id: id,
                artifact: artifact,
                group: dependency.module.id.group,
                name: dependency.module.id.name,
                version: dependency.module.id.version,
                extension: artifact.extension,
                componentId: artifact.id.componentIdentifier,
                children: Collections.unmodifiableList(new ArrayList<>(childModules)),
                licenseName: licenseName,
                licenseUrl: licenseUrl,
                licensePath: "",
                fileName: artifact.file.name,
                description: description,
                url: pomContent.url?.text(),
                displayName: pomContent.name?.text(),
                exclude: false,
        ))
    }

    private customizeDep(DependencyDescription dep) {
        if (dep.id?.startsWith("com_google_android_gms_play_services_")) {
            dep.licenseUrl = ""
            // This should match fetch_all._ANDROID_SDK_LICENSE_PATH
            dep.licensePath = "Android_SDK_License-December_9_2016.txt"
            if (dep.url?.isEmpty()) {
                dep.url = "https://developers.google.com/android/guides/setup"
            }
            // Filter out targets like:
            //     com_google_android_gms_play_services_auth_api_phone_license
            if (dep.id?.endsWith("_license")) {
                dep.exclude = true
            }
        } else if (dep.licenseName?.isEmpty()) {
            def fallbackProperties = FALLBACK_PROPERTIES.get(id)
            if (fallbackProperties != null) {
                project.logger.debug("Using fallback properties for ${id}")
                dep.licenseName = fallbackProperties.licenseName
                dep.licenseUrl = fallbackProperties.licenseUrl
            }
        }

        return dep
    }

    private resolveLicenseInformation(String id, GPathResult pomContent) {
      def licenseName = ''
      def licenseUrl = ''

      def error = ''
      GPathResult licenses = pomContent?.licenses?.license
      if (!licenses || licenses.size() == 0) {
          error = "No license found on ${id}"
      } else if (licenses.size() > 1) {
          error = "More than one license found on ${id}"
      }

      if (error.isEmpty()) return [licenses[0].name.text(), licenses[0].url.text()]

      project.logger.warn(error)
      return ['', '']
    }

    static class DependencyDescription {
        String id
        ResolvedArtifact artifact
        String group, name, version, extension, displayName, description, url
        String licenseName, licenseUrl, licensePath
        String fileName
        boolean supportsAndroid, visible, exclude
        ComponentIdentifier componentId
        List<String> children
    }
}
